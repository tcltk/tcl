if {$argc != 1} {
    puts stderr "Usage: [info script] <path-to-tclsh>"
    exit 1
}

set tclsh_path [lindex $argv 0]

# Normalize path separators to forward slashes for manipulation
set normalized [file normalize $tclsh_path]

# Get the directory containing the executable (removes bin\tclsh91.exe)
set bin_dir [file dirname $normalized]
set install_dir [file dirname $bin_dir]

# Build the target path: <install_dir>/lib/nmake/nmakehlp.exe
set nmakehlp_path [file nativename [file join $install_dir lib nmake nmakehlp.exe]]

if {[file exists $nmakehlp_path]} {
    puts "NMAKEHLP_NATIVE=$nmakehlp_path"
    exit 0
} else {
    puts stderr "Error: nmakehlp.exe not found at: $nmakehlp_path"
    exit 1
}
