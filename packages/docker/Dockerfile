FROM ubuntu:24.04
MAINTAINER SereneDB

RUN apt-get update && apt-get install -y --no-install-recommends numactl && apt-get autoremove -y --purge && apt-get clean

ADD install.tar.gz /
COPY setup.sh /setup.sh
RUN /setup.sh
RUN rm /setup.sh

ENV GLIBCXX_FORCE_NEW=1

# Adjust TZ by default since tzdata package isn't present
RUN echo "UTC" > /etc/timezone

# Note that Openshift runs containers by default with a random UID and GID 0.
# We need that the database and apps directory are writable for this config.
RUN chgrp -R 0 /var/lib/serenedb && \
    chmod -R 775 /var/lib/serenedb

# retain the database directory
VOLUME ["/var/lib/serenedb"]

COPY entrypoint.sh /entrypoint.sh
RUN ["chmod", "+x", "/entrypoint.sh"]
ENTRYPOINT ["/entrypoint.sh"]

EXPOSE 8529
CMD ["serened"]
