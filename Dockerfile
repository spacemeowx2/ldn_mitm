FROM devkitpro/devkita64_devkitarm:20190720

# RUN dkp-pacman -Sy --noconfirm libnx
RUN git clone -b master https://github.com/switchbrew/libnx.git --depth=1 \
    && cd libnx \
    && make install \
    && cd .. \
    && rm -rf libnx

VOLUME [ "/code" ]
WORKDIR /code

CMD [ "make", "-j8" ]
