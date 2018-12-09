# dirigent-multicore

How to build and run it?
---
```bash
git submodule init
git submodule update
cd ${DIRIGENT_HOME}/cmr/pcm
make
cd ${DIRIGENT_HOME}/cmr
make
```

To run dirigent with freq. scale and dynamic partition enabled.
```bash
sudo ./cmr ${cfg} freq part
```

or To run dirigent to get offline solorun data
```bash
sudo ./cmr ${cfg} no no
```