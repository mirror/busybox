#!/bin/bash
docker run --rm -v $PWD:/app -w /app ghcr.io/panda-re/embedded-toolchains:latest "/app/clean.sh"
