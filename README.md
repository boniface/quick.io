# Quick.IO

QIO is a very fast, very simple WebSocket server written entirely in C. Unlike other options, such as Socket.IO, that endeavor to be care-free, fun, and feature-rich, QIO care-expensive, difficult, and not-too-featureful. The one advantage QIO has over all the others, though, is that it is fast. It's so fast even its name is fast.

## How fast are we talking?

In initial tests, running on a Large EC2 instance, QIO broke 1.5 million concurrent connections (from actual clients in the wild), used 450MB ram, and had a load average of .5.  The server never even broke a sweat.

## Why the name?

Originally, the project was called Q.IO, named after the omnipotent Q from Star Trek, implying that it gave your IO the power of Q. No one seemed to get the reference, so it just became easier to tell everyone it stood for "Quick".

## Requirements

* linux >= 2.6.30
* glib >= 2.32
* openssl >= 1.0.0
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
sudo sysctl -w fs.epoll.max_user_watches=4194304
sudo sysctl -w fs.file-max=4197304
sudo sysctl -w fs.nr_open=4197304
sudo sysctl -w net.core.somaxconn=1024
sudo sysctl -w net.ipv4.tcp_mem="2820736 3031648 3533472"
```

Don't forget to update the number of files you're allowed to have open.

## Coding Style

Most of the ideas are taken from: https://www.kernel.org/doc/Documentation/CodingStyle. As long as your code fits in, we're good.
