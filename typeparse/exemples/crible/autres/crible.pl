use Time::HiRes qw(time); my $N=1000000;
sub best{my $f=shift;my $b=1e18;my $r;for(1..5){my $t=time;$r=$f->();my $d=time-$t;$b=$d if $d<$b}return($b*1000,$r)}
my ($tc,$s)=best(sub{[(1)x$N]});
my ($tk,$s2)=best(sub{my @a=@$s;$a[0]=$a[1]=0;for(my $i=2;$i*$i<$N;$i++){if($a[$i]){for(my $j=$i*$i;$j<$N;$j+=$i){$a[$j]=0}}}\@a});
my ($tn,$r)=best(sub{my $c=0;$c+=$_ for @$s2;$c});
printf("%-32s %7.1f %7.1f %7.1f %7.1f -> %d\n","Perl tableau + boucle",$tc,$tk,$tn,$tc+$tk+$tn,$r);
