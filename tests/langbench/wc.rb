def wordsplit(line)
    list = []
    word = ""
    line.split('').each do |c|
	if c =~ /\s/
	    if word.length > 0
		list << word
	    end
	    word = ""
	else
	    word += c
	end
    end
    if word.length > 0
	list << word
    end
    return list
end

n = 0
while line = gets()
    words = wordsplit(line)
    n += words.length
end
printf("%d\n", n)
