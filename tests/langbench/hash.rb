hash = {}
while line = gets()
        hash[line] = 1
end
   
fd = File.open("/proc/#{$$}/status")
while $_ = fd.gets
        print if $_ =~ /^Vm[RD]/
end
fd.close
