/* C-lang interface to DXVK's C++ implementation */

#include "config.h"

extern "C" {

#include "dxvk.h"

void dxvk_get_options(struct DXVKOptions *opts)
{
    dxvk::Config config(dxvk::Config::getUserConfig());

    config.merge(dxvk::Config::getAppConfig(dxvk::getExePath()));

    opts->nvapiHack = config.getOption<bool>("dxgi.nvapiHack", true) ? 1 : 0;
    opts->customVendorId = dxvk::parsePciId(config.getOption<std::string>("dxgi.customVendorId"));
    opts->customDeviceId = dxvk::parsePciId(config.getOption<std::string>("dxgi.customDeviceId"));
}

}
