# Quick.io (aka QIO)

## Requirements

* glib >= 2.32
* openssl >= 1.0
* doxygen + sphinx (for generating docs)

### Debian-based systems

```bash
# For compiling
sudo aptitude install build-essential libglib2.0-dev libmemcached-dev libssl-dev
```

## Testing

With check installed, run:

```bash
make test
```