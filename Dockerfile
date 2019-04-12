FROM devkitpro/devkita64_devkitarm:20190212

RUN dkp-pacman -Sy --noconfirm libnx
# RUN git clone -b v2.1.0 https://github.com/switchbrew/libnx.git --depth=1 \
#     && cd libnx \
#     && make install \
#     && cd .. \
#     && rm -rf libnx

VOLUME [ "/code" ]
WORKDIR /code

CMD [ "make", "-j8" ]
