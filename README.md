# DBPoolMySQL

DBPool mechanism using MySQL connector written in C.
An example project using [GwLib] of Kannel SMS Gateway project to create DBPool connections with MySQL.

### Init project

```sh
$ git clone https://github.com/dimimpou/DBPoolMySQL.git ./DBPoolMySQL
$ cd DBPoolMySQL
$ git submodule update --init
```


### Build [GwLib]

```sh
$ cd gwlib
$ ./configure --with-mysql
$ make
```

### Build project

```sh
$ cd ..
$ make
```

[GwLib]:https://github.com/dimimpou/gwlib.git