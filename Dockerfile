FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    openmpi-bin \
    libopenmpi-dev \
    openssh-client \
    openssh-server \
    && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /var/run/sshd
RUN echo 'root:root' | chpasswd
RUN sed -i 's/#PermitRootLogin prohibit-password/PermitRootLogin yes/' /etc/ssh/sshd_config
RUN sed -i 's/#PubkeyAuthentication yes/PubkeyAuthentication yes/' /etc/ssh/sshd_config

RUN mkdir -p /root/.ssh && chmod 700 /root/.ssh
RUN ssh-keygen -t ed25519 -f /root/.ssh/id_ed25519 -q -N ""
RUN cp /root/.ssh/id_ed25519.pub /root/.ssh/authorized_keys
RUN echo "Host *\n\tStrictHostKeyChecking no\n" >> /root/.ssh/config

RUN chmod 600 /root/.ssh/authorized_keys /root/.ssh/id_ed25519 /root/.ssh/config

ENV OMPI_ALLOW_RUN_AS_ROOT=1
ENV OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

WORKDIR /app

CMD ["/usr/sbin/sshd", "-D"]
