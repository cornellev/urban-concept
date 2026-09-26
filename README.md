# urban-concept

Monorepo for Cornell Electric Vehicles' Urban Concept car

## Getting started

Clone, then fetch the pico-sdk submodule and the one nested submodule it needs:

```sh
git clone https://github.com/cornellev/urban-concept.git
cd urban-concept
git submodule update --init
git -C vendor/pico-sdk submodule update --init lib/tinyusb
```

## Electrical

See [elec/README.md](elec/README.md)

## Telemetry

See [telem/README.md](telem/README.md)

## Autonomy

See [auto/README.md](auto/README.md)

## Shared libraries

See [cev-lib/README.md](cev-lib/README.md)
