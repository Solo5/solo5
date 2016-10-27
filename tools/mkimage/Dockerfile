FROM alpine:latest
MAINTAINER Martin Lucina <martin@lucina.net>
RUN apk add --update --no-cache \
    coreutils \
    dosfstools \
    mtools \
    sfdisk \
    syslinux \
    tar
COPY ./solo5-mkimage.sh /sbin
ENTRYPOINT [ "/sbin/solo5-mkimage.sh" ]
