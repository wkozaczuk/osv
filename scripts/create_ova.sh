IMAGE_NAME=$1
cp build/release.x64/usr.img ${IMAGE_NAME}.img
qemu-img convert -O vmdk ${IMAGE_NAME}.img ${IMAGE_NAME}.vmdk
./scripts/gen-vmx.sh ${IMAGE_NAME}
rm ${IMAGE_NAME}.ova 
ovftool ${IMAGE_NAME}.vmx ${IMAGE_NAME}.ova
