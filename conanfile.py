from conans import ConanFile

class BlobTreeConan(ConanFile):
    name = "blob_tree"
    version = "0.1"
    license = "MIT"
    author = "Tony Di Croce dicroce@gmail.com"
    url = "https://github.com/dicroce/blob_tree.git"
    description = "The goal of blob_tree is to bring the heirarchical tree like structure of JSON to the world of binary data."
    exports_sources = "blob_tree.h"
    no_copy_source = True

    def package(self):
        self.copy("*.h")
