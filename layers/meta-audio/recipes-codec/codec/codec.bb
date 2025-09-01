SUMMARY = "Audio Codec Test"
SECTION = "code"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = " \
    file://codec.c \
    file://sinewave1.pcm \
    file://sinewave2.wav \
    file://49.wav \
    file://50.wav \
    file://51.wav \
    file://52.wav \
    file://53.wav \
    file://54.wav \
    file://60.wav \
    file://65.wav \
    file://66.wav \
    file://69.wav \
"

S = "${WORKDIR}"

DEPENDS += " libopus liblc3"

do_compile() {
         ${CC} ${CFLAGS} ${LDFLAGS} codec.c -o codec -lopus -llc3 -lm
}

do_install() {
         install -d ${D}${bindir}
         install -m 0755 codec ${D}${bindir}

         install -d ${D}${datadir}/${PN}
         install -m 0644 ${WORKDIR}/sinewave1.pcm ${D}${datadir}/${PN}/
         install -m 0644 ${WORKDIR}/sinewave2.wav ${D}${datadir}/${PN}/
         install -m 0644 ${WORKDIR}/49.wav ${D}${datadir}/${PN}/
         install -m 0644 ${WORKDIR}/50.wav ${D}${datadir}/${PN}/
         install -m 0644 ${WORKDIR}/51.wav ${D}${datadir}/${PN}/
         install -m 0644 ${WORKDIR}/52.wav ${D}${datadir}/${PN}/
         install -m 0644 ${WORKDIR}/53.wav ${D}${datadir}/${PN}/
         install -m 0644 ${WORKDIR}/54.wav ${D}${datadir}/${PN}/
         install -m 0644 ${WORKDIR}/60.wav ${D}${datadir}/${PN}/
         install -m 0644 ${WORKDIR}/65.wav ${D}${datadir}/${PN}/
         install -m 0644 ${WORKDIR}/66.wav ${D}${datadir}/${PN}/
         install -m 0644 ${WORKDIR}/69.wav ${D}${datadir}/${PN}/
}


FILES:${PN} = "${bindir}/codec ${datadir}/${PN}/*"