#!/bin/bash
set -e -o -x

while getopts r:a:l:c:s:t: parameter_Option
do case "${parameter_Option}"
in
r) BINARY_DIR=${OPTARG};;
a) ARTIFACT_NAME=${OPTARG};;
l) LIB_NAME=${OPTARG};;
c) BUILD_CONFIG=${OPTARG};;
s) SOURCE_DIR=${OPTARG};;
t) COMMIT_ID=${OPTARG};;
esac
done

EXIT_CODE=1

uname -a
mkdir $BINARY_DIR/$ARTIFACT_NAME
mkdir $BINARY_DIR/$ARTIFACT_NAME/lib
mkdir $BINARY_DIR/$ARTIFACT_NAME/include
echo "Directories created"
cp $BINARY_DIR/$BUILD_CONFIG/$LIB_NAME $BINARY_DIR/$ARTIFACT_NAME/lib
echo "Copy debug symbols in a separate file and strip the original binary."
if [[ $LIB_NAME == *.dylib ]]
then
    dsymutil $BINARY_DIR/$ARTIFACT_NAME/lib/$LIB_NAME -o $BINARY_DIR/$ARTIFACT_NAME/lib/$LIB_NAME.dSYM
    strip -S $BINARY_DIR/$ARTIFACT_NAME/lib/$LIB_NAME
fi
cp $SOURCE_DIR/include/onnxruntime/core/session/onnxruntime_c_api.h  $BINARY_DIR/$ARTIFACT_NAME/include
# copy the README, licence and TPN
cp $SOURCE_DIR/README.md $BINARY_DIR/$ARTIFACT_NAME/README.md
cp $SOURCE_DIR/docs/C_API.md $BINARY_DIR/$ARTIFACT_NAME/C_API.md
cp $SOURCE_DIR/LICENSE $BINARY_DIR/$ARTIFACT_NAME/LICENSE
cp $SOURCE_DIR/ThirdPartyNotices.txt $BINARY_DIR/$ARTIFACT_NAME/ThirdPartyNotices.txt
cp $SOURCE_DIR/VERSION_NUMBER $BINARY_DIR/$ARTIFACT_NAME/VERSION_NUMBER
echo $COMMIT_ID > $BINARY_DIR/$ARTIFACT_NAME/GIT_COMMIT_ID

EXIT_CODE=$?

set -e
exit $EXIT_CODE
