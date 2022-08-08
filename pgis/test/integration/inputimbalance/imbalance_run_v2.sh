
buildDir=$1
outDir=$2
testNo=$3
binary=${buildDir}/tool/pgis_pira

echo -e "Load imbalance Selection\nRunning $binary"
echo -e "For now skipping MetaCG file format 2 test"
cubeTest="${testNo/mcg/cubex}"
#echo "${binary} --metacg-format 2 --out-file $outDir --cube ${PWD}/$cubeTest --parameter-file ${PWD}/parameters.json --lide 1 --debug 1 ${PWD}/$testNo"
${binary} --metacg-format 2 --out-file $outDir --cube ${PWD}/$cubeTest --parameter-file ${PWD}/parameters.json --lide 1 --debug 1 ${PWD}/$testNo 
