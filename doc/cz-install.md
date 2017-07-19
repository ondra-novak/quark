# Instalace na cílovém stroji 

 1. Předpokládejme že máme k dispozici adresář "inst" vytvořeného během sestavení (cz-build). Obsah adresáře lze zabalit do balíčku, nebo přes scp poslat na cílový stroj.
 2. Soubory nějak dopravíme na cílový stroj, například do složky /tmp/inst
 
## Instalace balíčků

```bash
$ sudo apt install couchdb xinetd curl
```

## Zpřístupnění couchdb na webovém rozhraní (volitelné)

Produkční stroj by neměl mít přístupné webové rozhraní, ale pokud je to třeba, existují dvě cesty
 1. Povolit couchdb otevřít port na všech síťových rozhraní: 
  ```bash
  $ sudo nano /etc/couchdb/local.ini
  # Odkomentovat řádek bind_address a místo 127.0.0.1 napsat 0.0.0.0
```
     * webové rozhraní je pak přístupné na url: http://<adresa_stroje>:5984/_utils

 2. Použít ssh s port forwardingem a protunelovat port 5984 (doporučeno). 
     * Je třeba ale nainstalovat openssh-server (většnou už je)
     * webové rozhraní je pak přístupné na url: http://localhost:5984/_utils
     
## Vypnutí admin party (nastavení admin účtu)

Admin účet lze nastavit ve webovém rozhraní, nebo z příkazové řádky (na cílovém stroji)

```bash
$ curl -s -X PUT http://localhost:5984/_config/admins/admin - '"iamroot"'
```
 * Výše uvedený řádek vytváří účet admin a nastavuje heslo na "iamroot". Je možné zvolit jiné údaje, jen pozor na to, že některé konfiguráky mají tento login zadaný jako výchozí, tedy je nutné configurák upravit
 * pro založení dalších admin-účtu je třeba jméno a heslo prvního admina jako basic authentification (nebo použít webové rozhraní)
 
## Instalace aplikace

```bash
$ sudo cp -rv /tmp/inst/* /
```

## Přidání marketu

Matching server hostí jeden až více marketů. Každý market je třeba nakonfigurovat. K tomu slouží program **quark_init**

  1. vygenerovat si šablonu configu: 
 
  ```bash
   $ quark_init > init.conf
```
  
  2. upravit config
  
  ```bash
   $ nano init.conf
```

  * poznámky
      * V configu je většina cesta nastavená tak, aby odpovídala instalačním cestám. Ale pokud se instalační cesty změní, je třeba opravit config
      * Login a heslo na admina, pokud je jiné
      * Login a heslo na daemona a rpc, pokud jsou změněny, je třeba upravit configy /usr/local/etc/quark/, kde jsou také tyto přihlašovací údaje uvedeny
         * (jen pro vysvětlení, proč jsou dva účty)
         * daemon má právo konfigurovat databáze
         * rpc má právo pouze zapisovat příkazy a číst stav,
      * Nastavení **marketName** je v celku důležitý název, protože slouží jak reference na jiných službách
      * Nastavení **currency** představuje jméno měny, kterou se platí
      * Nastavení **asset** představuje jméno měny, se kterou se obchoduje
      * Nastavení **granuality** určuje nejmenší jednotku množství (ve kterém se zadává množství)
      * Nastavení **pipSize** určuje nejmenší jednotku ceny (ve které se zadává cena)
      * V configu jsou další nastavení, jejichž význam lze odvodit z názvu. Všechny je třeba vyplnit a dát si pozor, aby výsledkem byl validní json (čárky v configu, na konci čárka není)
      
   3. spustit:
   
   ```bash
   $ sudo quark_init init.conf
```

   * Program by měl provést sérii kroků vedoucí k nastavení potřebných konfigů pro spuštění daného marketu
   * Při prvním běhu zakládá i uživatele, při opakovaném běhu pouze informuje, že již existují (není to chyba)
   * je ho potřeba spustit jako root (zapisuje konfiguraci do /etc)
   * pro market založí 3 prázdné databáze (objeví se ve webovém rozhraní)
   * do databáze -orders vloží objekt "settings" ve kterém je podrobné nastavení trhu
   
   4. restart couchdb
   
   ```bash
   $sudo service couchdb restart
```
  
   * Restartem couchdb se spustí daemon matchingu
   * v databázích přibude několik _design dokumentů
   * v databází "-orders" nesmí být přítomen objekt "error"
   
### hotovo

## Odstranění marketu

  Pro odstranění marketu je potřeba zejména deaktivivat démona, který jej spravuje. To se provede v souboru /etc/couchdb/default.d/daemons.ini (jméno souboru se vypíše na závěr běhu programu quark_init) a v tomto souboru je třeba odstranit řádek, který nese jméno marketu. Poté se musí databáze restartovat a teprve pak lze odstranit jednotlivé databaze
  
  

