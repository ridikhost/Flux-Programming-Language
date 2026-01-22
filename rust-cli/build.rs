fn main() {
    let dst = cmake::Config::new("..")
        .define("CMAKE_BUILD_TYPE", "Release")
        .build();

    println!("cargo:rustc-link-search=native={}", dst.join("lib").display());
    println!("cargo:rustc-link-lib=static=fluxc");

    // libm only needed on some unix toolchains
    if std::env::var("CARGO_CFG_TARGET_OS").unwrap_or_default() != "windows" {
        println!("cargo:rustc-link-lib=m");
    }
}
