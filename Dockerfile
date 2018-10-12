FROM devkitpro/devkita64_devkitarm

VOLUME [ "/code" ]
WORKDIR /code

CMD [ "make", "-j8" ]
