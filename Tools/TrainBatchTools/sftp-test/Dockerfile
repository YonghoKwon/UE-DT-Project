# Local SFTP receiver for TRAIN-BATCH-01 delivery tests only.
# Official Alpine base + OpenSSH; key-only login for one user; upload dir /data/inbox.
FROM alpine:3.20

RUN apk add --no-cache openssh-server \
 && ssh-keygen -A \
 && adduser -D -s /bin/sh trainer \
 && passwd -u trainer \
 && mkdir -p /home/trainer/.ssh /data/inbox \
 && chown -R trainer:trainer /home/trainer /data \
 && chmod 700 /home/trainer/.ssh

COPY sshd_config /etc/ssh/sshd_config
# Tolerate a CRLF checkout of sshd_config on Windows.
RUN sed -i 's/\r$//' /etc/ssh/sshd_config && mkdir -p /etc/ssh/authorized_keys

EXPOSE 22
CMD ["/usr/sbin/sshd", "-D", "-e"]
