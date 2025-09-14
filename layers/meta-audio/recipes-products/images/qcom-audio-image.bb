require recipes-products/images/qcom-multimedia-image.bb

LICENSE = "BSD-3-Clause-Clear"

SUMMARY = "Audio compression dependencies" 

IMAGE_INSTALL:append = " liblc3 libopus codec packagegroup-core-buildessential alsa-utils alsa-lib qcom-pal qcom-pal-headers" 
IMAGE_INSTALL:remove = " fluoride"
