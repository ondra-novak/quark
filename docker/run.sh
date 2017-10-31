#!/bin/sh
CURRENCY_PAIR=$1
EXPOSED_PORT=$2

START_DIR="/srv/crypto-alpha/"

docker rm -f quark.${CURRENCY_PAIR} || /bin/true || /usr/bin/true

docker run --detach \
--name quark.${CURRENCY_PAIR} \
--restart always \
-p ${EXPOSED_PORT}:18123 \
-v ${START_DIR}/quark/services.d:/etc/services.d \
-v ${START_DIR}/quark/xinetd.d/quark:/etc/xinetd.d/quark \
-v ${START_DIR}/quark/quark:/etc/quark \
-v ${START_DIR}/quark/quark_bot.conf:/etc/quark/quark_bot.conf \
-v ${START_DIR}/quark/quark_init_answers/quark_init.answers.${CURRENCY_PAIR}:/tmp/quark_init.answers \
registry.simplifate.zlutazimnice.cz/quark:latest

# we have to wait until couchdb is initialized
sleep 5
docker exec -ti quark.${CURRENCY_PAIR} bash -c "/usr/local/bin/quark_init < /tmp/quark_init.answers"
