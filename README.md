# macb

Portable command-line tool for MacBinary archives.

## Build and install

```sh
$ make install
```

Don't run that as root; you'll be prompted for the password when needed.

Installs by default at `/usr/local/bin/macb`. Adjust `Makefile` if you want it elsewhere.

## Usage

```sh
# Get detailed info.
$ macb --help

# Extract whichever forks are present, simple case.
$ macb -x ExistingFile.bin

# Examine an archive: Is it MacBinary?
$ macb -t ExistingFile.bin

# Create an archive from existing forks.
$ macb -c NewFile.bin -d ExistingDataFile -r ExistingResourceFile -T "FlTp" -C "Crtr"
```

If you want to control Finder flags, timestamps, etc, you can also provide a partial 128-byte header.
`macb -c` will overwrite only the fork lengths and CRC in that case.
