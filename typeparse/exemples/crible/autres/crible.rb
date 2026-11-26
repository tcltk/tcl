N=1000000
def best; b=1e18; r=nil; 5.times{ t=Process.clock_gettime(Process::CLOCK_MONOTONIC); r=yield; d=Process.clock_gettime(Process::CLOCK_MONOTONIC)-t; b=d if d<b }; [b*1000,r]; end
tc,s=best{ Array.new(N,1) }
tk,s2=best{ a=s.dup; a[0]=a[1]=0; i=2; while i*i<N; if a[i]==1 then j=i*i; while j<N; a[j]=0; j+=i; end; end; i+=1; end; a }
tn,r=best{ s2.sum }
printf("%-32s %7.1f %7.1f %7.1f %7.1f -> %d\n","Ruby 3.3 Array + boucle",tc,tk,tn,tc+tk+tn,r)
