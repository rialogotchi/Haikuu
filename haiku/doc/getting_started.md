# Getting Started with Haiku

In this guide we will go through the steps required to set up a tiny user-mode
Haiku instance. When run as a regular user, Haiku can be used to supervise,
start, and stop other processes (services) also running under the same user ID.

We assume that Haiku has already been installed; i.e. we will only cover
configuration here. See [../BUILD](../BUILD) for information on how to
build Haiku from source and install it.

We don't cover how to set up Haiku as a system "init". See [linux/HAIKU-AS-INIT.md](linux/HAIKU-AS-INIT.md)
if you're interested in using Haiku as your system "init" or service manager.
However, you may still wish to read through this guide first, as it will give
an overview of Haiku's configuration and operation.

## Starting Haiku

The main component of Haiku is the `haiku` daemon. Let's start by running haiku
with the bare minimum configuration. First we need to choose a place for our
service descriptions to live. For this guide we are going to use the default
location (for user-mode), which is `~/.config/haiku.d`.

```
$ mkdir ~/.config/haiku.d
$ cd ~/.config/haiku.d
```

Next we need a service description for the `boot` service. This is the service
that haiku will bring up upon starting.

Create (using your preferred text editor) a file called `boot` (under the
directory we just made) and put as its contents:
```
type = internal
waits-for.d: boot.d
```

The first line (`type = internal`) tells haiku that this is not a service which
will run any external process directly.

The second line (`waits-for.d: boot.d`) specifies a directory which will be
used to keep track of which services should be started when haiku starts (or
more specifically, which services should be started before the `boot` service).
Here we've made it a `boot.d` sub-directory under our main `haiku.d` directory,
but it can be anywhere you like so long as you adjust the `waits-for.d` setting
appropriately.

Now, let's create the aforementioned directory and start haiku for the first time.
Assuming you are still in `~/.config/haiku.d`:
```
$ mkdir boot.d
$ haiku
[  OK  ] boot
```

Note: you may need to specify the full path to `haiku`, such as `/sbin/haiku`,
depending on details of your system and how Haiku was installed.

Haiku lives!

The `[  OK  ] boot` message tells us that the `boot` service we created has
been started. Note that haiku always starts the boot service by default,
unless you specify a different service or services on the command line.

In the long run, you'll want to find a way to invoke haiku at boot time, but
that's an exercise for the reader. For the moment, leave haiku running and
switch to another terminal. Or, if you would prefer to stick to one terminal,
press Ctrl+C (or the interrupt combination for your terminal) and haiku should
gracefully exit, then start haiku again, this time in the background:
```
$ haiku -q & 
```

Because `-q` suppresses service status messages, you won't see `[  OK  ] boot` as
you did when running haiku the first time.

## Adding Service Descriptions

So we have haiku running, but it currently has no services to supervise. Let's
give it something to do.

Suppose we want to run mpd, the music player daemon, under haiku. Put the
following in ` ~/.config/haiku.d/mpd`:
```
type = process
command = /usr/local/sbin/mpd --no-daemon
restart = true
```

This assumes, of course, that you have a suitable `mpd` at the specified path.
If you don't have mpd installed, you can use "sleep" for testing purposes;
change the `command` line above to:
```
command = /usr/bin/sleep 600
```
(you should double check the location of 'sleep' first; it may be in '/bin'!)

Now run `haikuctl list` (or `/sbin/haikuctl list`):
```
$ haikuctl list
[[+]     ] boot
```

The mpd service isn't visible yet because haiku lazily loads services. If we
start the service, we will see it in the list:
```
$ haikuctl start mpd
Service 'mpd' started.
$ haikuctl list
[[+]     ] boot
[[+]     ] mpd (pid: 14823)
```

Now let's simulate mpd crashing and check haiku brings it back up:
```
$ kill 14823
```

On the haiku log, we see:
```
[STOPPD] mpd
[  OK  ] mpd
```

And if we query haiku for its status, we see:
```
$ haikuctl list
[[+]     ] boot
[[+]     ] mpd (pid: 1667)
```

Notice that a new instance of mpd is running; it has a different pid.

You can stop a service using `haikuctl stop`:
```
$ haikuctl stop mpd
Service 'mpd' stopped.
$ haikuctl list
[[+]     ] boot
[     {-}] mpd
```

Here the "slider" for the mpd service has been moved to the right to signify
that it has been switched off.

## Starting Services at Startup

So far we've configured a service which can be brought up and down in an ad-hoc
fashion. This would be ideal for SSH tunnels, for example, but mpd is the kind
of daemon you want to *always* run; we want it to start when haiku itself
starts.

To that end, we can use `haikuctl enable mpd`. This will start the service
immediately *and* make sure it starts by default:
```
$ haikuctl list
[[+]     ] boot
[     {-}] mpd
$ haikuctl enable mpd
Service 'mpd' has been enabled.
$ haikuctl list
[[+]     ] boot
[{+}     ] mpd (pid: 49921)
```

Notice that the mpd status is shown as `{+}` rather than `[+]` as it was
earlier. This is because it is now started only as a dependency of boot -
we haven't explicitly marked it active (as is done via `haikuctl start`).
This means that if boot stops, mpd will also stop (and haiku will also
stop, seeing as it has no services left running).

We now want to restart haiku, to see that the mpd service does indeed start
automatically. First, stop haiku:
```
$ haikuctl shutdown
Shutting down haiku...
Connection closed.
```
(you could also send it the TERM signal using the `kill` command, or press Ctrl+C
in the terminal where it is running in the foreground; or, as alluded earlier, you
could stop the boot service via `haikuctl stop boot`).

Start haiku again (`haiku` in the other terminal, or `haiku -q &`, as before).
Then list services to make sure mpd started:
```
$ haikuctl list
[[+]     ] boot
[{+}     ] mpd (pid: 17601)
```

Success! - mpd was started when haiku started.

If you look in the `waits-for.d` directory we configured earlier, you will
find a symlink to the mpd service description file. This is how haiku keeps
track of what should be started by default.

## Further Reading

In this guide, we've really only scratched the surface of what Haiku can do.
For example, we've hardly touched on dependencies (where one service depends
upon another to function). For full details about service configuration, read
the `haiku-service(5)` manual page. The `haiku`, `haikuctl` and `haiku-check`
commands also have manual pages.
