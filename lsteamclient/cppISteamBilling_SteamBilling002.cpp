/* This file is auto-generated, do not edit. */
#include "unix_private.h"

#if 0
#pragma makedep unix
#endif

NTSTATUS ISteamBilling_SteamBilling002_InitCreditCardPurchase( void *args )
{
    struct ISteamBilling_SteamBilling002_InitCreditCardPurchase_params *params = (struct ISteamBilling_SteamBilling002_InitCreditCardPurchase_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->InitCreditCardPurchase( params->nPackageID, params->nCardIndex, params->bStoreCardInfo );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_InitCreditCardPurchase( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_InitCreditCardPurchase_params *params = (struct wow64_ISteamBilling_SteamBilling002_InitCreditCardPurchase_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->InitCreditCardPurchase( params->nPackageID, params->nCardIndex, params->bStoreCardInfo );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_InitPayPalPurchase( void *args )
{
    struct ISteamBilling_SteamBilling002_InitPayPalPurchase_params *params = (struct ISteamBilling_SteamBilling002_InitPayPalPurchase_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->InitPayPalPurchase( params->nPackageID );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_InitPayPalPurchase( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_InitPayPalPurchase_params *params = (struct wow64_ISteamBilling_SteamBilling002_InitPayPalPurchase_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->InitPayPalPurchase( params->nPackageID );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetActivationCodeInfo( void *args )
{
    struct ISteamBilling_SteamBilling002_GetActivationCodeInfo_params *params = (struct ISteamBilling_SteamBilling002_GetActivationCodeInfo_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetActivationCodeInfo( params->pchActivationCode );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetActivationCodeInfo( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetActivationCodeInfo_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetActivationCodeInfo_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetActivationCodeInfo( params->pchActivationCode );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_PurchaseWithActivationCode( void *args )
{
    struct ISteamBilling_SteamBilling002_PurchaseWithActivationCode_params *params = (struct ISteamBilling_SteamBilling002_PurchaseWithActivationCode_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->PurchaseWithActivationCode( params->pchActivationCode );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_PurchaseWithActivationCode( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_PurchaseWithActivationCode_params *params = (struct wow64_ISteamBilling_SteamBilling002_PurchaseWithActivationCode_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->PurchaseWithActivationCode( params->pchActivationCode );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetFinalPrice( void *args )
{
    struct ISteamBilling_SteamBilling002_GetFinalPrice_params *params = (struct ISteamBilling_SteamBilling002_GetFinalPrice_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetFinalPrice(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetFinalPrice( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetFinalPrice_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetFinalPrice_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetFinalPrice(  );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_CancelPurchase( void *args )
{
    struct ISteamBilling_SteamBilling002_CancelPurchase_params *params = (struct ISteamBilling_SteamBilling002_CancelPurchase_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->CancelPurchase(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_CancelPurchase( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_CancelPurchase_params *params = (struct wow64_ISteamBilling_SteamBilling002_CancelPurchase_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->CancelPurchase(  );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_CompletePurchase( void *args )
{
    struct ISteamBilling_SteamBilling002_CompletePurchase_params *params = (struct ISteamBilling_SteamBilling002_CompletePurchase_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->CompletePurchase(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_CompletePurchase( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_CompletePurchase_params *params = (struct wow64_ISteamBilling_SteamBilling002_CompletePurchase_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->CompletePurchase(  );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_UpdateCardInfo( void *args )
{
    struct ISteamBilling_SteamBilling002_UpdateCardInfo_params *params = (struct ISteamBilling_SteamBilling002_UpdateCardInfo_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->UpdateCardInfo( params->nCardIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_UpdateCardInfo( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_UpdateCardInfo_params *params = (struct wow64_ISteamBilling_SteamBilling002_UpdateCardInfo_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->UpdateCardInfo( params->nCardIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_DeleteCard( void *args )
{
    struct ISteamBilling_SteamBilling002_DeleteCard_params *params = (struct ISteamBilling_SteamBilling002_DeleteCard_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->DeleteCard( params->nCardIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_DeleteCard( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_DeleteCard_params *params = (struct wow64_ISteamBilling_SteamBilling002_DeleteCard_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->DeleteCard( params->nCardIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetCardList( void *args )
{
    struct ISteamBilling_SteamBilling002_GetCardList_params *params = (struct ISteamBilling_SteamBilling002_GetCardList_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetCardList(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetCardList( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetCardList_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetCardList_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetCardList(  );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_Obsolete_GetLicenses( void *args )
{
    struct ISteamBilling_SteamBilling002_Obsolete_GetLicenses_params *params = (struct ISteamBilling_SteamBilling002_Obsolete_GetLicenses_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->Obsolete_GetLicenses(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_Obsolete_GetLicenses( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_Obsolete_GetLicenses_params *params = (struct wow64_ISteamBilling_SteamBilling002_Obsolete_GetLicenses_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->Obsolete_GetLicenses(  );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_CancelLicense( void *args )
{
    struct ISteamBilling_SteamBilling002_CancelLicense_params *params = (struct ISteamBilling_SteamBilling002_CancelLicense_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->CancelLicense( params->nPackageID, params->nCancelReason );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_CancelLicense( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_CancelLicense_params *params = (struct wow64_ISteamBilling_SteamBilling002_CancelLicense_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->CancelLicense( params->nPackageID, params->nCancelReason );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetPurchaseReceipts( void *args )
{
    struct ISteamBilling_SteamBilling002_GetPurchaseReceipts_params *params = (struct ISteamBilling_SteamBilling002_GetPurchaseReceipts_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetPurchaseReceipts( params->bUnacknowledgedOnly );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetPurchaseReceipts( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetPurchaseReceipts_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetPurchaseReceipts_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetPurchaseReceipts( params->bUnacknowledgedOnly );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_SetBillingAddress( void *args )
{
    struct ISteamBilling_SteamBilling002_SetBillingAddress_params *params = (struct ISteamBilling_SteamBilling002_SetBillingAddress_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->SetBillingAddress( params->nCardIndex, params->pchFirstName, params->pchLastName, params->pchAddress1, params->pchAddress2, params->pchCity, params->pchPostcode, params->pchState, params->pchCountry, params->pchPhone );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_SetBillingAddress( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_SetBillingAddress_params *params = (struct wow64_ISteamBilling_SteamBilling002_SetBillingAddress_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->SetBillingAddress( params->nCardIndex, params->pchFirstName, params->pchLastName, params->pchAddress1, params->pchAddress2, params->pchCity, params->pchPostcode, params->pchState, params->pchCountry, params->pchPhone );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetBillingAddress( void *args )
{
    struct ISteamBilling_SteamBilling002_GetBillingAddress_params *params = (struct ISteamBilling_SteamBilling002_GetBillingAddress_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetBillingAddress( params->nCardIndex, params->pchFirstName, params->pchLastName, params->pchAddress1, params->pchAddress2, params->pchCity, params->pchPostcode, params->pchState, params->pchCountry, params->pchPhone );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetBillingAddress( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetBillingAddress_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetBillingAddress_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetBillingAddress( params->nCardIndex, params->pchFirstName, params->pchLastName, params->pchAddress1, params->pchAddress2, params->pchCity, params->pchPostcode, params->pchState, params->pchCountry, params->pchPhone );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_SetShippingAddress( void *args )
{
    struct ISteamBilling_SteamBilling002_SetShippingAddress_params *params = (struct ISteamBilling_SteamBilling002_SetShippingAddress_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->SetShippingAddress( params->pchFirstName, params->pchLastName, params->pchAddress1, params->pchAddress2, params->pchCity, params->pchPostcode, params->pchState, params->pchCountry, params->pchPhone );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_SetShippingAddress( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_SetShippingAddress_params *params = (struct wow64_ISteamBilling_SteamBilling002_SetShippingAddress_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->SetShippingAddress( params->pchFirstName, params->pchLastName, params->pchAddress1, params->pchAddress2, params->pchCity, params->pchPostcode, params->pchState, params->pchCountry, params->pchPhone );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetShippingAddress( void *args )
{
    struct ISteamBilling_SteamBilling002_GetShippingAddress_params *params = (struct ISteamBilling_SteamBilling002_GetShippingAddress_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetShippingAddress( params->pchFirstName, params->pchLastName, params->pchAddress1, params->pchAddress2, params->pchCity, params->pchPostcode, params->pchState, params->pchCountry, params->pchPhone );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetShippingAddress( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetShippingAddress_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetShippingAddress_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetShippingAddress( params->pchFirstName, params->pchLastName, params->pchAddress1, params->pchAddress2, params->pchCity, params->pchPostcode, params->pchState, params->pchCountry, params->pchPhone );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_SetCardInfo( void *args )
{
    struct ISteamBilling_SteamBilling002_SetCardInfo_params *params = (struct ISteamBilling_SteamBilling002_SetCardInfo_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->SetCardInfo( params->nCardIndex, params->eCreditCardType, params->pchCardNumber, params->pchCardHolderFirstName, params->pchCardHolderLastName, params->pchCardExpYear, params->pchCardExpMonth, params->pchCardCVV2 );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_SetCardInfo( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_SetCardInfo_params *params = (struct wow64_ISteamBilling_SteamBilling002_SetCardInfo_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->SetCardInfo( params->nCardIndex, params->eCreditCardType, params->pchCardNumber, params->pchCardHolderFirstName, params->pchCardHolderLastName, params->pchCardExpYear, params->pchCardExpMonth, params->pchCardCVV2 );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetCardInfo( void *args )
{
    struct ISteamBilling_SteamBilling002_GetCardInfo_params *params = (struct ISteamBilling_SteamBilling002_GetCardInfo_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetCardInfo( params->nCardIndex, params->eCreditCardType, params->pchCardNumber, params->pchCardHolderFirstName, params->pchCardHolderLastName, params->pchCardExpYear, params->pchCardExpMonth, params->pchCardCVV2 );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetCardInfo( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetCardInfo_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetCardInfo_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetCardInfo( params->nCardIndex, params->eCreditCardType, params->pchCardNumber, params->pchCardHolderFirstName, params->pchCardHolderLastName, params->pchCardExpYear, params->pchCardExpMonth, params->pchCardCVV2 );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetLicensePackageID( void *args )
{
    struct ISteamBilling_SteamBilling002_GetLicensePackageID_params *params = (struct ISteamBilling_SteamBilling002_GetLicensePackageID_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetLicensePackageID( params->nLicenseIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetLicensePackageID( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetLicensePackageID_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetLicensePackageID_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetLicensePackageID( params->nLicenseIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetLicenseTimeCreated( void *args )
{
    struct ISteamBilling_SteamBilling002_GetLicenseTimeCreated_params *params = (struct ISteamBilling_SteamBilling002_GetLicenseTimeCreated_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetLicenseTimeCreated( params->nLicenseIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetLicenseTimeCreated( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetLicenseTimeCreated_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetLicenseTimeCreated_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetLicenseTimeCreated( params->nLicenseIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetLicenseTimeNextProcess( void *args )
{
    struct ISteamBilling_SteamBilling002_GetLicenseTimeNextProcess_params *params = (struct ISteamBilling_SteamBilling002_GetLicenseTimeNextProcess_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetLicenseTimeNextProcess( params->nLicenseIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetLicenseTimeNextProcess( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetLicenseTimeNextProcess_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetLicenseTimeNextProcess_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetLicenseTimeNextProcess( params->nLicenseIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetLicenseMinuteLimit( void *args )
{
    struct ISteamBilling_SteamBilling002_GetLicenseMinuteLimit_params *params = (struct ISteamBilling_SteamBilling002_GetLicenseMinuteLimit_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetLicenseMinuteLimit( params->nLicenseIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetLicenseMinuteLimit( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetLicenseMinuteLimit_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetLicenseMinuteLimit_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetLicenseMinuteLimit( params->nLicenseIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetLicenseMinutesUsed( void *args )
{
    struct ISteamBilling_SteamBilling002_GetLicenseMinutesUsed_params *params = (struct ISteamBilling_SteamBilling002_GetLicenseMinutesUsed_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetLicenseMinutesUsed( params->nLicenseIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetLicenseMinutesUsed( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetLicenseMinutesUsed_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetLicenseMinutesUsed_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetLicenseMinutesUsed( params->nLicenseIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetLicensePaymentMethod( void *args )
{
    struct ISteamBilling_SteamBilling002_GetLicensePaymentMethod_params *params = (struct ISteamBilling_SteamBilling002_GetLicensePaymentMethod_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetLicensePaymentMethod( params->nLicenseIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetLicensePaymentMethod( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetLicensePaymentMethod_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetLicensePaymentMethod_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetLicensePaymentMethod( params->nLicenseIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetLicenseFlags( void *args )
{
    struct ISteamBilling_SteamBilling002_GetLicenseFlags_params *params = (struct ISteamBilling_SteamBilling002_GetLicenseFlags_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetLicenseFlags( params->nLicenseIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetLicenseFlags( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetLicenseFlags_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetLicenseFlags_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetLicenseFlags( params->nLicenseIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetLicensePurchaseCountryCode( void *args )
{
    struct ISteamBilling_SteamBilling002_GetLicensePurchaseCountryCode_params *params = (struct ISteamBilling_SteamBilling002_GetLicensePurchaseCountryCode_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetLicensePurchaseCountryCode( params->nLicenseIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetLicensePurchaseCountryCode( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetLicensePurchaseCountryCode_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetLicensePurchaseCountryCode_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetLicensePurchaseCountryCode( params->nLicenseIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetReceiptPackageID( void *args )
{
    struct ISteamBilling_SteamBilling002_GetReceiptPackageID_params *params = (struct ISteamBilling_SteamBilling002_GetReceiptPackageID_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptPackageID( params->nReceiptIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetReceiptPackageID( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetReceiptPackageID_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetReceiptPackageID_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptPackageID( params->nReceiptIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetReceiptStatus( void *args )
{
    struct ISteamBilling_SteamBilling002_GetReceiptStatus_params *params = (struct ISteamBilling_SteamBilling002_GetReceiptStatus_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptStatus( params->nReceiptIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetReceiptStatus( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetReceiptStatus_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetReceiptStatus_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptStatus( params->nReceiptIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetReceiptResultDetail( void *args )
{
    struct ISteamBilling_SteamBilling002_GetReceiptResultDetail_params *params = (struct ISteamBilling_SteamBilling002_GetReceiptResultDetail_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptResultDetail( params->nReceiptIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetReceiptResultDetail( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetReceiptResultDetail_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetReceiptResultDetail_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptResultDetail( params->nReceiptIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetReceiptTransTime( void *args )
{
    struct ISteamBilling_SteamBilling002_GetReceiptTransTime_params *params = (struct ISteamBilling_SteamBilling002_GetReceiptTransTime_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptTransTime( params->nReceiptIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetReceiptTransTime( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetReceiptTransTime_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetReceiptTransTime_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptTransTime( params->nReceiptIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetReceiptTransID( void *args )
{
    struct ISteamBilling_SteamBilling002_GetReceiptTransID_params *params = (struct ISteamBilling_SteamBilling002_GetReceiptTransID_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptTransID( params->nReceiptIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetReceiptTransID( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetReceiptTransID_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetReceiptTransID_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptTransID( params->nReceiptIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetReceiptPaymentMethod( void *args )
{
    struct ISteamBilling_SteamBilling002_GetReceiptPaymentMethod_params *params = (struct ISteamBilling_SteamBilling002_GetReceiptPaymentMethod_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptPaymentMethod( params->nReceiptIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetReceiptPaymentMethod( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetReceiptPaymentMethod_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetReceiptPaymentMethod_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptPaymentMethod( params->nReceiptIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetReceiptBaseCost( void *args )
{
    struct ISteamBilling_SteamBilling002_GetReceiptBaseCost_params *params = (struct ISteamBilling_SteamBilling002_GetReceiptBaseCost_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptBaseCost( params->nReceiptIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetReceiptBaseCost( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetReceiptBaseCost_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetReceiptBaseCost_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptBaseCost( params->nReceiptIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetReceiptTotalDiscount( void *args )
{
    struct ISteamBilling_SteamBilling002_GetReceiptTotalDiscount_params *params = (struct ISteamBilling_SteamBilling002_GetReceiptTotalDiscount_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptTotalDiscount( params->nReceiptIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetReceiptTotalDiscount( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetReceiptTotalDiscount_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetReceiptTotalDiscount_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptTotalDiscount( params->nReceiptIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetReceiptTax( void *args )
{
    struct ISteamBilling_SteamBilling002_GetReceiptTax_params *params = (struct ISteamBilling_SteamBilling002_GetReceiptTax_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptTax( params->nReceiptIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetReceiptTax( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetReceiptTax_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetReceiptTax_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptTax( params->nReceiptIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetReceiptShipping( void *args )
{
    struct ISteamBilling_SteamBilling002_GetReceiptShipping_params *params = (struct ISteamBilling_SteamBilling002_GetReceiptShipping_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptShipping( params->nReceiptIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetReceiptShipping( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetReceiptShipping_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetReceiptShipping_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptShipping( params->nReceiptIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetReceiptCountryCode( void *args )
{
    struct ISteamBilling_SteamBilling002_GetReceiptCountryCode_params *params = (struct ISteamBilling_SteamBilling002_GetReceiptCountryCode_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptCountryCode( params->nReceiptIndex );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetReceiptCountryCode( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetReceiptCountryCode_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetReceiptCountryCode_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetReceiptCountryCode( params->nReceiptIndex );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetNumLicenses( void *args )
{
    struct ISteamBilling_SteamBilling002_GetNumLicenses_params *params = (struct ISteamBilling_SteamBilling002_GetNumLicenses_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetNumLicenses(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetNumLicenses( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetNumLicenses_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetNumLicenses_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetNumLicenses(  );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetNumReceipts( void *args )
{
    struct ISteamBilling_SteamBilling002_GetNumReceipts_params *params = (struct ISteamBilling_SteamBilling002_GetNumReceipts_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetNumReceipts(  );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetNumReceipts( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetNumReceipts_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetNumReceipts_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetNumReceipts(  );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_PurchaseWithMachineID( void *args )
{
    struct ISteamBilling_SteamBilling002_PurchaseWithMachineID_params *params = (struct ISteamBilling_SteamBilling002_PurchaseWithMachineID_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->PurchaseWithMachineID( params->nPackageID, params->pchCustomData );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_PurchaseWithMachineID( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_PurchaseWithMachineID_params *params = (struct wow64_ISteamBilling_SteamBilling002_PurchaseWithMachineID_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->PurchaseWithMachineID( params->nPackageID, params->pchCustomData );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_InitClickAndBuyPurchase( void *args )
{
    struct ISteamBilling_SteamBilling002_InitClickAndBuyPurchase_params *params = (struct ISteamBilling_SteamBilling002_InitClickAndBuyPurchase_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->InitClickAndBuyPurchase( params->nPackageID, params->nAccountNum, params->pchState, params->pchCountryCode );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_InitClickAndBuyPurchase( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_InitClickAndBuyPurchase_params *params = (struct wow64_ISteamBilling_SteamBilling002_InitClickAndBuyPurchase_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->InitClickAndBuyPurchase( params->nPackageID, params->nAccountNum, params->pchState, params->pchCountryCode );
    return 0;
}
#endif

NTSTATUS ISteamBilling_SteamBilling002_GetPreviousClickAndBuyAccount( void *args )
{
    struct ISteamBilling_SteamBilling002_GetPreviousClickAndBuyAccount_params *params = (struct ISteamBilling_SteamBilling002_GetPreviousClickAndBuyAccount_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetPreviousClickAndBuyAccount( params->pnAccountNum, params->pchState, params->pchCountryCode );
    return 0;
}

#if defined(__x86_64__) || defined(__aarch64__)
NTSTATUS wow64_ISteamBilling_SteamBilling002_GetPreviousClickAndBuyAccount( void *args )
{
    struct wow64_ISteamBilling_SteamBilling002_GetPreviousClickAndBuyAccount_params *params = (struct wow64_ISteamBilling_SteamBilling002_GetPreviousClickAndBuyAccount_params *)args;
    struct u_ISteamBilling_SteamBilling002 *iface = (struct u_ISteamBilling_SteamBilling002 *)params->u_iface;
    params->_ret = iface->GetPreviousClickAndBuyAccount( params->pnAccountNum, params->pchState, params->pchCountryCode );
    return 0;
}
#endif

