<?php $N=1000000;
function best($f){$b=1e18;$r=null;for($i=0;$i<5;$i++){$t=hrtime(true);$r=$f();$d=(hrtime(true)-$t)/1e6;if($d<$b)$b=$d;}return [$b,$r];}
[$tc,$s]=best(fn()=>array_fill(0,$N,1));
[$tk,$s2]=best(function()use($s,$N){$a=$s;$a[0]=$a[1]=0;for($i=2;$i*$i<$N;$i++)if($a[$i])for($j=$i*$i;$j<$N;$j+=$i)$a[$j]=0;return $a;});
[$tn,$r]=best(fn()=>array_sum($s2));
printf("%-32s %7.1f %7.1f %7.1f %7.1f -> %d\n","PHP 8.3 tableau + boucle",$tc,$tk,$tn,$tc+$tk+$tn,$r);
