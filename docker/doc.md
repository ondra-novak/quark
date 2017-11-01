## run multiple quarks in containers

```
/srv/crypto-alpha/quark/run.sh btceth 18124
/srv/crypto-alpha/quark/run.sh btceur 18125
/srv/crypto-alpha/quark/run.sh btcltc 18126
/srv/crypto-alpha/quark/run.sh btcdash 18127
/srv/crypto-alpha/quark/run.sh btcdoge 18128
/srv/crypto-alpha/quark/run.sh btcxmr 18129
```

The idea is to mount config file into running contaner. As can be seen in `run.sh`

## todo
- if we expect to run quark-bots inside containers we need to provide specific configurations for each of them