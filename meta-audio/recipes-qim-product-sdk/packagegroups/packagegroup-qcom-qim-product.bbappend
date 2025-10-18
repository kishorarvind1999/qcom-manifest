# Remove the non-existent file from SRC_URI in the original recipe in 
# meta-qcom-qim-product-sdk/recipes-qim-product-sdk/packagegroups/packagegroup-qcom-qim-product.bb
SRC_URI:remove = "file://install.sh"