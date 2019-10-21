/* C-lang interface to DXVK's C++ implementation */

struct DXVKOptions {
    int32_t customVendorId;
    int32_t customDeviceId;
    int nvapiHack;
};

void dxvk_get_options(struct DXVKOptions *);
