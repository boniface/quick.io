# Quick.io (aka QIO)

## Requirements

* glib >= 2.32
* openssl >= 1.0
* doxygen + sphinx (for generating docs)

### Debian-based systems

```bash
# For compiling
sudo aptitude install build-essential libglib2.0-dev libssl-dev
```

## Testing

With check installed, run:

```bash
make test
```

## Configuring your Kernel

If you're going to have a crap-ton (yes, that is a metric) of clients connected, you're going to need to tune some kernel parameters.

```bash
sudo sysctl -w net.ipv4.tcp_mem="202304 259072 392608"
echo 2000000 | sudo tee /proc/sys/fs/file-max
```

Don't forget to update the number of files you're allowed to have open.