#ifndef ISTEAMBILLING_H
#define ISTEAMBILLING_H
#ifdef _WIN32
#pragma once
#endif

#include "isteamclient.h"

typedef int32 EPaymentMethod;
typedef int32 EPurchaseStatus;
typedef int32 EPurchaseResultDetail;

class ISteamBilling
{
public:
    virtual bool InitCreditCardPurchase( int32 nPackageID, uint32 nCardIndex, bool bStoreCardInfo );
    virtual bool InitPayPalPurchase( int32 nPackageID );
    virtual bool GetActivationCodeInfo(const char *pchActivationCode );
    virtual bool PurchaseWithActivationCode( const char *pchActivationCode );
    virtual bool GetFinalPrice();
    virtual bool CancelPurchase();
    virtual bool CompletePurchase();
    virtual bool UpdateCardInfo( uint32 nCardIndex );
    virtual bool DeleteCard( uint32 nCardIndex );
    virtual bool GetCardList();
    virtual bool Obsolete_GetLicenses();
    virtual bool CancelLicense( int32 nPackageID, int32 nCancelReason );
    virtual bool GetPurchaseReceipts( bool bUnacknowledgedOnly );
    virtual bool SetBillingAddress( uint32 nCardIndex, const char *pchFirstName, const char *pchLastName, const char *pchAddress1, const char *pchAddress2, const char *pchCity, const char *pchPostcode, const char *pchState, const char *pchCountry, const char *pchPhone );
    virtual bool GetBillingAddress( uint32 nCardIndex, char *pchFirstName, char *pchLastName, char *pchAddress1, char *pchAddress2, char *pchCity, char *pchPostcode, char *pchState, char *pchCountry, char *pchPhone );
    virtual bool SetShippingAddress( const char *pchFirstName, const char *pchLastName, const char *pchAddress1, const char *pchAddress2, const char *pchCity, const char *pchPostcode, const char *pchState, const char *pchCountry, const char *pchPhone );
    virtual bool GetShippingAddress( char *pchFirstName, char *pchLastName, char *pchAddress1, char *pchAddress2, char *pchCity, char *pchPostcode, char *pchState, char *pchCountry, char *pchPhone );
    virtual bool SetCardInfo( uint32 nCardIndex, int32 eCreditCardType, const char *pchCardNumber, const char *pchCardHolderFirstName, const char *pchCardHolderLastName, const char *pchCardExpYear, const char *pchCardExpMonth, const char *pchCardCVV2 );
    virtual bool GetCardInfo( uint32 nCardIndex, int32  *eCreditCardType, char *pchCardNumber, char *pchCardHolderFirstName, char *pchCardHolderLastName, char *pchCardExpYear, char *pchCardExpMonth, char *pchCardCVV2 );
    virtual int32 GetLicensePackageID( uint32 nLicenseIndex );
    virtual uint32 GetLicenseTimeCreated( uint32 nLicenseIndex );
    virtual uint32 GetLicenseTimeNextProcess( uint32 nLicenseIndex );
    virtual int32 GetLicenseMinuteLimit( uint32 nLicenseIndex );
    virtual int32 GetLicenseMinutesUsed( uint32 nLicenseIndex );
    virtual EPaymentMethod GetLicensePaymentMethod( uint32 nLicenseIndex );
    virtual uint32  GetLicenseFlags( uint32 nLicenseIndex );
    virtual const char * GetLicensePurchaseCountryCode( uint32 nLicenseIndex );
    virtual int32  GetReceiptPackageID( uint32 nReceiptIndex );
    virtual EPurchaseStatus GetReceiptStatus( uint32 nReceiptIndex );
    virtual EPurchaseResultDetail GetReceiptResultDetail( uint32 nReceiptIndex );
    virtual uint32  GetReceiptTransTime( uint32 nReceiptIndex );
    virtual uint64  GetReceiptTransID( uint32 nReceiptIndex );
    virtual EPaymentMethod GetReceiptPaymentMethod( uint32 nReceiptIndex );
    virtual uint32  GetReceiptBaseCost( uint32 nReceiptIndex );
    virtual uint32  GetReceiptTotalDiscount( uint32 nReceiptIndex );
    virtual uint32  GetReceiptTax( uint32 nReceiptIndex );
    virtual uint32  GetReceiptShipping( uint32 nReceiptIndex );
    virtual const char * GetReceiptCountryCode( uint32 nReceiptIndex );
    virtual uint32  GetNumLicenses();
    virtual uint32  GetNumReceipts();
    virtual bool PurchaseWithMachineID( int32 nPackageID, const char *pchCustomData );
    virtual bool InitClickAndBuyPurchase( int32 nPackageID, int64 nAccountNum, const char *pchState, const char *pchCountryCode );
    virtual bool GetPreviousClickAndBuyAccount( int64  *pnAccountNum, char *pchState, char *pchCountryCode );
};

#define STEAMBILLING_INTERFACE_VERSION "SteamBilling002"

#endif
