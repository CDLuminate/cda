#!/bin/bash
# archivemount wrapper which implements cda

export TEMPDIR=`mktemp -d`
export SHELL="bash"

set -e

clean () {
	if [ -d $TEMPDIR ]; then 
		if ! $(mountpoint ${TEMPDIR}) ; then
			fusermount -u ${TEMPDIR};
		fi
		rmdir ${TEMPDIR};
	fi
}
trap clean EXIT	

archivemount -o ro -o nonempty $@ ${TEMPDIR} 
(cd ${TEMPDIR}; ${SHELL})
