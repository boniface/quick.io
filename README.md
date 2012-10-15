# Quick.io (aka QIO)

## Requirements

* glib >= 2.32
* doxygen + sphinx (for generating docs)

### Debian-based systems

```bash
# For compiling
sudo aptitude install build-essential libglib2.0-dev libmemcached-dev

# For testing
sudo aptitude install gawk
```

## Testing

With check installed, run:

```bash
make test
```