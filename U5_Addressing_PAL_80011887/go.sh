#!/bin/bash -xe
PAL=$1
FPREFIX=NabuAdapter_${PAL}_800-11887
IOMASK=$2

die () {
    echo >&2 "$@"
    exit 1
}

echo $PAL | grep -E -q '^16R[68]$' || die "PAL type needs to be 16R6 or 16R8, '$PAL' provided"
#echo $IOMASK | grep -E -q '^[0-9A-F][0-9A-F]$' || die "I/O mask needs to be two hex characters, '$IOMASK' provided"

if [ -z $IOMASK ]; then
	java -jar ~/dupal/DuPAL_Analyzer/target/dupal-analyzer-0.1.4-jar-with-dependencies.jar /dev/ttyUSB0 ${PAL} ${FPREFIX}.json $IOMASK
	exit 0
fi

java -jar ~/dupal/DuPAL_Analyzer/target/dupal-analyzer-0.1.4-jar-with-dependencies.jar /dev/ttyUSB0 ${PAL} ${FPREFIX}.json $IOMASK
java -jar ~/dupal/DuPAL_EspressoConverter/target/espresso-converter-0.0.3-jar-with-dependencies.jar ${FPREFIX}.json ${FPREFIX}.espresso
if [ -f ${FPREFIX}.espresso ]; then
	~/dupal/espresso-logic/bin/espresso -o eqntott ${FPREFIX}.espresso > ${FPREFIX}.txt
else
	~/dupal/espresso-logic/bin/espresso -o eqntott ${FPREFIX}.espresso.tbl0 > ${FPREFIX}.tbl0.txt
	~/dupal/espresso-logic/bin/espresso -o eqntott ${FPREFIX}.espresso.tbl1 > ${FPREFIX}.tbl1.txt
fi
