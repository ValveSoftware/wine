#!/bin/sh
set -e
tools/make_requests
tools/make_specfiles
dlls/winevulkan/make_vulkan -x vk.xml -X video.xml
autoreconf -ifv
rm -rf autom4te.cache

echo "Now run ./configure"
