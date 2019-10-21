/*
                  Copyright (c) 2017-2019 Philip Rebohle

                          zlib/libpng license

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

– The origin of this software must not be misrepresented; you must not
  claim that you wrote the original software. If you use this software
  in a product, an acknowledgment in the product documentation would be
  appreciated but is not required.

– Altered source versions must be plainly marked as such, and must not
  be misrepresented as being the original software.

– This notice may not be removed or altered from any source distribution.
*/

  const static std::vector<std::pair<const char*, Config>> g_appDefaults = {{
    /* Assassin's Creed Syndicate: amdags issues  */
    { R"(\\ACS\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Dissidia Final Fantasy NT Free Edition */
    { R"(\\dffnt\.exe$)", {{
      { "dxgi.deferSurfaceCreation",        "True" },
    }} },
    /* Elite Dangerous: Compiles weird shaders    *
     * when running on AMD hardware               */
    { R"(\\EliteDangerous64\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* The Vanishing of Ethan Carter Redux        */
    { R"(\\EthanCarter-Win64-Shipping\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* The Evil Within: Submits command lists     * 
     * multiple times                             */
    { R"(\\EvilWithin(Demo)?\.exe$)", {{
      { "d3d11.dcSingleUseMode",            "False" },
    }} },
    /* Far Cry 3: Assumes clear(0.5) on an UNORM  *
     * format to result in 128 on AMD and 127 on  *
     * Nvidia. We assume that the Vulkan drivers  *
     * match the clear behaviour of D3D11.        */
    { R"(\\(farcry3|fc3_blooddragon)_d3d11\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Far Cry 4: Same as Far Cry 3               */
    { R"(\\FarCry4\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Far Cry Primal: Nvidia performance         */
    { R"(\\FCPrimal\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    } }},
    /* Frostpunk: Renders one frame with D3D9     *
     * after creating the DXGI swap chain         */
    { R"(\\Frostpunk\.exe$)", {{
      { "dxgi.deferSurfaceCreation",        "True" },
    }} },
    /* Nioh: See Frostpunk, apparently?           */
    { R"(\\nioh\.exe$)", {{
      { "dxgi.deferSurfaceCreation",        "True" },
    }} },
    /* Quantum Break: Mever initializes shared    *
     * memory in one of its compute shaders       */
    { R"(\\QuantumBreak\.exe$)", {{
      { "d3d11.zeroInitWorkgroupMemory",    "True" },
    }} },
    /* Anno 2205: Random crashes with state cache */
    { R"(\\anno2205\.exe$)", {{
      { "dxvk.enableStateCache",            "False" },
    }} },
    /* Fifa '19+: Binds typed buffer SRV to shader *
     * that expects raw/structured buffer SRV     */
    { R"(\\FIFA(19|[2-9][0-9])(_demo)?\.exe$)", {{
      { "dxvk.useRawSsbo",                  "True" },
    }} },
    /* Final Fantasy XIV: Fix random black blocks */
    { R"(\\ffxiv_dx11\.exe$)", {{
      { "d3d11.strictDivision",             "True" },
    }} },
    /* Resident Evil 2: Improve GPU performance   */
    { R"(\\re2\.exe$)", {{
      { "d3d11.relaxedBarriers",            "True" },
    }} },
    /* Resident Evil 7                            */
    { R"(\\re7\.exe$)", {{
      { "d3d11.relaxedBarriers",            "True" },
    }} },
    /* Devil May Cry 5                            */
    { R"(\\DevilMayCry5\.exe$)", {{
      { "d3d11.relaxedBarriers",            "True" },
    }} },
    /* Call of Duty WW2                           */
    { R"(\\s2_sp64_ship\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Need for Speed 2015                        */
    { R"(\\NFS16\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Mass Effect Andromeda                      */
    { R"(\\MassEffectAndromeda\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Mirror`s Edge Catalyst: Crashes on AMD     */
    { R"(\\MirrorsEdgeCatalyst(Trial)?\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Star Wars Battlefront (2015)               */
    { R"(\\starwarsbattlefront(trial)?\.exe$)", {{
      { "dxgi.nvapiHack",                   "False" },
    }} },
    /* Dark Souls Remastered                      */
    { R"(\\DarkSoulsRemastered\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* Grim Dawn                                  */
    { R"(\\Grim Dawn\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* NieR:Automata                              */
    { R"(\\NieRAutomata\.exe$)", {{
      { "d3d11.constantBufferRangeCheck",   "True" },
    }} },
    /* SteamVR performance test                   */
    { R"(\\vr\.exe$)", {{
      { "d3d11.dcSingleUseMode",            "False" },
    }} },
    /* Overwatch                                  *
     * Broken for at least one user for unknown   *
     * reasons, seems to work for others. Just    *
     * disable DO_NOT_WAIT to be on the safe side */
     { R"(\\Overwatch\.exe$)", {{
      { "d3d11.allowMapFlagNoWait",         "False" },
     }} },
    /* Hitman 2 - requires AGS library            */
    { R"(\\HITMAN2\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
    /* Modern Warfare Remastered                  */
    { R"(\\h1_[ms]p64_ship\.exe$)", {{
      { "dxgi.customVendorId",              "10de" },
    }} },
  }};
