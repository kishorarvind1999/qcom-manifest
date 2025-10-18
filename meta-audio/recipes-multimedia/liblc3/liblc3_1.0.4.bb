SUMMARY = "Low Complexity Communication Codec (LC3)"
HOMEPAGE = "https://github.com/google/liblc3"
LICENSE = "Apache-2.0"
LIC_FILES_CHKSUM = "file://LICENSE;md5=3b83ef96387f14655fc854ddc3c6bd57"

SRC_URI = "git://github.com/google/liblc3.git;protocol=https;branch=main"

S = "${WORKDIR}/git"
SRCREV = "ce2e41faf8c06d038df9f32504c61109a14130be"

inherit pkgconfig meson

BBCLASSEXTEND = "native nativesdk"