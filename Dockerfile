FROM registry.access.redhat.com/ubi9/ubi AS builder

RUN dnf -y update \
    && dnf -y install \
        binutils \
        gcc \
        gcc-c++ \
        make \
        cmake \
        python3 \
        python3-pip \
        git \
    && dnf clean all

RUN python3 -m pip install --no-cache-dir conan

WORKDIR /opt/dlx
COPY . .

RUN conan config install conan \
    && conan profile detect --force \
    && conan install . -s build_type=Release --build=missing \
    && conan build . -s build_type=Release \
    && strip /opt/dlx/build/dlx \
    && mkdir -p /opt/dlx/runtime-libs \
    && ldd /opt/dlx/build/dlx | awk '{for (i=1;i<=NF;i++) if (index($i,"/")==1) print $i}' | sort -u > /opt/dlx/runtime-libs/ldd.list \
    && xargs -a /opt/dlx/runtime-libs/ldd.list -I{} cp -v --parents {} /opt/dlx/runtime-libs

FROM registry.access.redhat.com/ubi9/ubi-micro

COPY --from=builder /opt/dlx/build/dlx /usr/local/bin/dlx
COPY --from=builder /opt/dlx/runtime-libs/lib/ /lib/
COPY --from=builder /opt/dlx/runtime-libs/lib64/ /lib64/
COPY --from=builder /opt/dlx/scripts/dlx-entrypoint.sh /usr/local/bin/dlx-entrypoint.sh

RUN chmod +x /usr/local/bin/dlx-entrypoint.sh

ENV DLX_PROBLEM_PORT=7000 \
    DLX_SOLUTION_PORT=7001

EXPOSE 7000 7001

ENTRYPOINT ["/usr/local/bin/dlx-entrypoint.sh"]
