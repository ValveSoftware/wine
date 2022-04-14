#include <stdarg.h>

#define NONAMELESSUNION
#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winioctl.h"

#include "ddk/wdm.h"

#include "wine/debug.h"
#include "wine/list.h"
#include "wine/server.h"

WINE_DEFAULT_DEBUG_CHANNEL(sharedgpures);

static DRIVER_OBJECT *sharedgpures_driver;

struct shared_resource
{
    unsigned int ref_count;
    void *unix_resource;
    WCHAR *name;
    void *metadata;
    SIZE_T metadata_size;
};

static struct shared_resource *resource_pool;
static unsigned int resource_pool_size;

/* TODO: If/when ntoskrnl gets support for referencing user handles directly, remove this function */
static void *reference_client_handle(obj_handle_t handle)
{
    HANDLE client_process, kernel_handle;
    OBJECT_ATTRIBUTES attr;
    void *object = NULL;
    CLIENT_ID cid;

    attr.Length = sizeof(OBJECT_ATTRIBUTES);
    attr.RootDirectory = 0;
    attr.Attributes = OBJ_KERNEL_HANDLE;
    attr.ObjectName = NULL;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    cid.UniqueProcess = PsGetCurrentProcessId();
    cid.UniqueThread = 0;

    if (NtOpenProcess(&client_process, PROCESS_ALL_ACCESS, &attr, &cid) != STATUS_SUCCESS)
        return NULL;

    if (NtDuplicateObject(client_process, wine_server_ptr_handle(handle), NtCurrentProcess(), &kernel_handle,
                               0, OBJ_KERNEL_HANDLE, DUPLICATE_SAME_ACCESS) != STATUS_SUCCESS)
    {
        NtClose(client_process);
        return NULL;
    }

    ObReferenceObjectByHandle(kernel_handle, 0, NULL, KernelMode, &object, NULL);

    NtClose(client_process);
    NtClose(kernel_handle);

    return object;
}

#define IOCTL_SHARED_GPU_RESOURCE_CREATE           CTL_CODE(FILE_DEVICE_VIDEO, 0, METHOD_BUFFERED, FILE_WRITE_ACCESS)

struct shared_resource_create
{
    obj_handle_t unix_handle;
    WCHAR name[1];
};

static NTSTATUS shared_resource_create(struct shared_resource **res, void *buff, SIZE_T insize, IO_STATUS_BLOCK *iosb)
{
    struct shared_resource_create *input = buff;
    void *unix_resource;
    unsigned int i;
    LPWSTR name;

    if (insize < sizeof(*input))
        return STATUS_INFO_LENGTH_MISMATCH;

    if (input->name[ ((insize - offsetof(struct shared_resource_create, name)) / sizeof(WCHAR)) - 1 ])
        return STATUS_INVALID_PARAMETER;

    if (!(unix_resource = reference_client_handle(input->unix_handle)))
        return STATUS_INVALID_HANDLE;

    if (insize == sizeof(*input))
        name = NULL;
    else
    {
        name = ExAllocatePoolWithTag(NonPagedPool, insize - offsetof(struct shared_resource_create, name), 0);
        wcscpy(name, &input->name[0]);
    }

    for (i = 0; i < resource_pool_size; i++)
        if (!resource_pool[i].ref_count)
            break;

    if (i == resource_pool_size)
    {
        struct shared_resource *expanded_pool =
            ExAllocatePoolWithTag(NonPagedPool, sizeof(struct shared_resource) * (resource_pool_size + 1024), 0);

        if (resource_pool)
        {
            memcpy(expanded_pool, resource_pool, resource_pool_size * sizeof(struct shared_resource));
            ExFreePoolWithTag(resource_pool, 0);
        }

        memset(&expanded_pool[resource_pool_size], 0, 1024 * sizeof (struct shared_resource));

        resource_pool = expanded_pool;
        resource_pool_size += 1024;
    }

    *res = &resource_pool[i];
    (*res)->ref_count = 1;
    (*res)->unix_resource = unix_resource;
    (*res)->name = name;

    iosb->Information = 0;
    return STATUS_SUCCESS;
}

#define IOCTL_SHARED_GPU_RESOURCE_OPEN             CTL_CODE(FILE_DEVICE_VIDEO, 1, METHOD_BUFFERED, FILE_WRITE_ACCESS)

struct shared_resource_open
{
    obj_handle_t kmt_handle;
    WCHAR name[1];
};

static unsigned int kmt_to_index(obj_handle_t kmt)
{
    if (!(kmt & 0x40000000) || (kmt - 2) % 4)
        return -1;
    return (((unsigned int) kmt & ~0x40000000) - 2) / 4;
}

static NTSTATUS shared_resource_open(struct shared_resource **res, void *buff, SIZE_T insize, IO_STATUS_BLOCK *iosb)
{
    struct shared_resource_open *input = buff;
    unsigned int i;

    if (insize < sizeof(*input))
        return STATUS_INFO_LENGTH_MISMATCH;

    if (input->kmt_handle)
    {
        if (kmt_to_index(input->kmt_handle) >= resource_pool_size)
            return STATUS_INVALID_HANDLE;

        *res = &resource_pool[kmt_to_index(input->kmt_handle)];
    }
    else
    {
        if (input->name[ ((insize - offsetof(struct shared_resource_open, name)) / sizeof(WCHAR)) - 1 ])
            return STATUS_INVALID_PARAMETER;

        /* name lookup */
        for (i = 0; i < resource_pool_size; i++)
        {
            if (!wcscmp(resource_pool[i].name, &input->name[0]))
            {
                *res = &resource_pool[i];
                break;
            }
        }

        if (i == resource_pool_size)
            return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    (*res)->ref_count++;
    iosb->Information = 0;
    return STATUS_SUCCESS;
}

#define IOCTL_SHARED_GPU_RESOURCE_GETKMT           CTL_CODE(FILE_DEVICE_VIDEO, 2, METHOD_BUFFERED, FILE_READ_ACCESS)

static obj_handle_t index_to_kmt(unsigned int idx)
{
    return (idx * 4 + 2) | 0x40000000;
}

static NTSTATUS shared_resource_getkmt(struct shared_resource *res, void *buff, SIZE_T outsize, IO_STATUS_BLOCK *iosb)
{
    if (outsize < sizeof(unsigned int))
        return STATUS_INFO_LENGTH_MISMATCH;

    *((unsigned int *)buff) = index_to_kmt(res - resource_pool);

    iosb->Information = sizeof(unsigned int);
    return STATUS_SUCCESS;
}

/* TODO: If/when ntoskrnl gets support for opening user handles directly, remove this function */
static obj_handle_t open_client_handle(void *object)
{
    HANDLE client_process, kernel_handle, handle = NULL;
    OBJECT_ATTRIBUTES attr;
    CLIENT_ID cid;

    attr.Length = sizeof(OBJECT_ATTRIBUTES);
    attr.RootDirectory = 0;
    attr.Attributes = OBJ_KERNEL_HANDLE;
    attr.ObjectName = NULL;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    cid.UniqueProcess = PsGetCurrentProcessId();
    cid.UniqueThread = 0;

    if (NtOpenProcess(&client_process, PROCESS_ALL_ACCESS, &attr, &cid) != STATUS_SUCCESS)
        return 0;

    if (ObOpenObjectByPointer(object, 0, NULL, GENERIC_ALL, NULL, KernelMode, &kernel_handle) != STATUS_SUCCESS)
    {
        NtClose(client_process);
        return 0;
    }

    NtDuplicateObject(NtCurrentProcess(), kernel_handle, client_process, &handle,
                        0, 0, DUPLICATE_SAME_ACCESS);

    NtClose(client_process);
    NtClose(kernel_handle);

    return wine_server_obj_handle(handle);
}

#define IOCTL_SHARED_GPU_RESOURCE_GET_UNIX_RESOURCE           CTL_CODE(FILE_DEVICE_VIDEO, 3, METHOD_BUFFERED, FILE_READ_ACCESS)

static NTSTATUS shared_resource_get_unix_resource(struct shared_resource *res, void *buff, SIZE_T outsize, IO_STATUS_BLOCK *iosb)
{
    if (outsize < sizeof(obj_handle_t))
        return STATUS_INFO_LENGTH_MISMATCH;

    *((obj_handle_t *)buff) = open_client_handle(res->unix_resource);

    iosb->Information = sizeof(obj_handle_t);
    return STATUS_SUCCESS;
}

#define IOCTL_SHARED_GPU_RESOURCE_SET_METADATA           CTL_CODE(FILE_DEVICE_VIDEO, 4, METHOD_BUFFERED, FILE_WRITE_ACCESS)

static NTSTATUS shared_resource_set_metadata(struct shared_resource *res, void *buff, SIZE_T insize, IO_STATUS_BLOCK *iosb)
{
    res->metadata = ExAllocatePoolWithTag(NonPagedPool, insize, 0);
    memcpy(res->metadata, buff, insize);
    res->metadata_size = insize;

    iosb->Information = 0;
    return STATUS_SUCCESS;
}

#define IOCTL_SHARED_GPU_RESOURCE_GET_METADATA           CTL_CODE(FILE_DEVICE_VIDEO, 5, METHOD_BUFFERED, FILE_READ_ACCESS)

static NTSTATUS shared_resource_get_metadata(struct shared_resource *res, void *buff, SIZE_T outsize, IO_STATUS_BLOCK *iosb)
{
    if (!res->metadata)
        return STATUS_NOT_FOUND;

    if (res->metadata_size > outsize)
        return STATUS_BUFFER_TOO_SMALL;

    memcpy(buff, res->metadata, res->metadata_size);
    iosb->Information = res->metadata_size;
    return STATUS_SUCCESS;
}

static NTSTATUS WINAPI dispatch_create(DEVICE_OBJECT *device, IRP *irp)
{
    irp->IoStatus.u.Status = STATUS_SUCCESS;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static NTSTATUS WINAPI dispatch_close(DEVICE_OBJECT *device, IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation(irp);
    struct shared_resource *res = stack->FileObject->FsContext;

    TRACE("Freeing shared resouce %p.\n", res);

    if (res)
    {
        res->ref_count--;
        if (!res->ref_count)
        {
            if (res->unix_resource)
            {
                /* TODO: see if its possible to destroy the object here (unlink?) */
                ObDereferenceObject(res->unix_resource);
                res->unix_resource = NULL;
            }
            if (res->metadata)
            {
                ExFreePoolWithTag(res->metadata, 0);
                res->metadata = NULL;
            }
        }
    }

    irp->IoStatus.u.Status = STATUS_SUCCESS;
    IoCompleteRequest(irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static NTSTATUS WINAPI dispatch_ioctl(DEVICE_OBJECT *device, IRP *irp)
{
    IO_STACK_LOCATION *stack = IoGetCurrentIrpStackLocation( irp );
    struct shared_resource **res = (struct shared_resource **) &stack->FileObject->FsContext;
    NTSTATUS status;

    TRACE( "ioctl %x insize %u outsize %u\n",
           stack->Parameters.DeviceIoControl.IoControlCode,
           stack->Parameters.DeviceIoControl.InputBufferLength,
           stack->Parameters.DeviceIoControl.OutputBufferLength );

    switch (stack->Parameters.DeviceIoControl.IoControlCode)
    {
        case IOCTL_SHARED_GPU_RESOURCE_CREATE:
            status = shared_resource_create( res,
                                      irp->AssociatedIrp.SystemBuffer,
                                      stack->Parameters.DeviceIoControl.InputBufferLength,
                                      &irp->IoStatus );
            break;
        case IOCTL_SHARED_GPU_RESOURCE_OPEN:
            status = shared_resource_open( res,
                                      irp->AssociatedIrp.SystemBuffer,
                                      stack->Parameters.DeviceIoControl.InputBufferLength,
                                      &irp->IoStatus );
            break;
        case IOCTL_SHARED_GPU_RESOURCE_GETKMT:
            status = shared_resource_getkmt( *res,
                                      irp->AssociatedIrp.SystemBuffer,
                                      stack->Parameters.DeviceIoControl.OutputBufferLength,
                                      &irp->IoStatus );
            break;
        case IOCTL_SHARED_GPU_RESOURCE_GET_UNIX_RESOURCE:
            status = shared_resource_get_unix_resource( *res,
                                      irp->AssociatedIrp.SystemBuffer,
                                      stack->Parameters.DeviceIoControl.OutputBufferLength,
                                      &irp->IoStatus );
            break;
        case IOCTL_SHARED_GPU_RESOURCE_SET_METADATA:
            status = shared_resource_set_metadata( *res,
                                      irp->AssociatedIrp.SystemBuffer,
                                      stack->Parameters.DeviceIoControl.InputBufferLength,
                                      &irp->IoStatus );
            break;
        case IOCTL_SHARED_GPU_RESOURCE_GET_METADATA:
            status = shared_resource_get_metadata( *res,
                                      irp->AssociatedIrp.SystemBuffer,
                                      stack->Parameters.DeviceIoControl.OutputBufferLength,
                                      &irp->IoStatus );
            break;
    default:
        FIXME( "ioctl %x not supported\n", stack->Parameters.DeviceIoControl.IoControlCode );
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    irp->IoStatus.u.Status = status;
    IoCompleteRequest( irp, IO_NO_INCREMENT );
    return status;
}

NTSTATUS WINAPI DriverEntry( DRIVER_OBJECT *driver, UNICODE_STRING *path )
{
    static const WCHAR device_nameW[] = L"\\Device\\SharedGpuResource";
    static const WCHAR link_nameW[] = L"\\??\\SharedGpuResource";
    UNICODE_STRING device_name, link_name;
    DEVICE_OBJECT *device;
    NTSTATUS status;

    sharedgpures_driver = driver;

    driver->MajorFunction[IRP_MJ_CREATE] = dispatch_create;
    driver->MajorFunction[IRP_MJ_CLOSE] = dispatch_close;
    driver->MajorFunction[IRP_MJ_DEVICE_CONTROL] = dispatch_ioctl;

    RtlInitUnicodeString(&device_name, device_nameW);
    RtlInitUnicodeString(&link_name, link_nameW);

    if ((status = IoCreateDevice(driver, 0, &device_name, 0, 0, FALSE, &device)))
        return status;

    return IoCreateSymbolicLink(&link_name, &device_name);
}
