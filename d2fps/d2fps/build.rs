fn main() {
  let exports =
    std::path::PathBuf::from(std::env::var_os("CARGO_MANIFEST_DIR").unwrap()).join("Win32.def");
  println!("cargo:rustc-link-arg-cdylib=/DEF:{}", exports.display());
  println!("cargo:rustc-link-arg-cdylib=/PDBALTPATH:d2fps.pdb");
  println!("cargo:rustc-link-arg-cdylib=/Brepro");
  println!("cargo:rerun-if-changed=Win32.def");
  embed_resource::compile("d2fps.rc", embed_resource::NONE);
  println!("cargo:rerun-if-changed=d2fps.rc");
}
