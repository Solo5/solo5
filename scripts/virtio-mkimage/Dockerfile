FROM alpine:3.8
MAINTAINER Martin Lucina <martin@lucina.net>
RUN apk add --update --no-cache \
    coreutils \
    dosfstools \
    mtools \
    sfdisk \
    syslinux \
    tar
COPY ./solo5-virtio-mkimage.sh /sbin
ENTRYPOINT [ "/sbin/solo5-virtio-mkimage.sh" ]
