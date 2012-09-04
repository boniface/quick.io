# Quick.io (aka QIO)

## Requirements

* glib >= 2.32
* libsoup >= 2.38 (will probably be replaced soon)
* check (only for running unit tests)

### Debian-based systems

```bash
# For compiling
sudo aptitude install build-essential libglib2.0-dev libsoup2.4-dev libmemcached-dev

# For testing
sudo aptitude install check gawk
```

## Testing

With check installed, run:

```bash
make test
```