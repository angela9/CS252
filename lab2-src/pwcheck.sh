#!/bin/bash

#DO NOT REMOVE THE FOLLOWING TWO LINES
git add $0 >> .local.git.out
git commit -a -m "Lab 2 commit" >> .local.git.out
git push >> .local.git.out || echo


#Your code here
IFS= read -r password < $1
echo $password

FILE=$1

#check password length

LENGTH=$(echo ${#password})
SCORE=$LENGTH

#check less than

if [ $LENGTH -lt 6 ]; then
	echo "Error: Password length invalid."
	exit 1
fi

#check greater than

if [ $LENGTH -gt 32 ]; then
	echo "Error: Password length invalid."
	exit 1
fi

#check special characters

if [ $(egrep -c [#$+%@] $FILE) -eq 1 ]; then
	let SCORE=SCORE+5;
fi

#check number

if [ $(egrep -c [0-9] $FILE) -eq 1 ]; then
	let SCORE=SCORE+5;
fi

#check alphabet

if [ $(egrep -c [A-Za-z] $FILE) -eq 1 ] ; then
	let SCORE=SCORE+5;
fi

#check alphanumeric

if [ $(egrep -c "([A-Za-z0-9])\1+" $FILE) -eq 1 ]; then
         let SCORE=SCORE-10;
fi
#check conscutive lower case

if [ $(egrep -c [a-z][a-z][a-z] $FILE) -eq 1 ]; then
	let SCORE=SCORE-3
fi

#check consecutive upper case

if [ $(egrep -c [A-Z][A-Z][A-Z] $FILE) -eq 1 ]; then
       let SCORE=SCORE-3
fi

#check consecutive numbers

if [ $(egrep -c [0-9][0-9][0-9] $FILE) -eq 1 ]; then
	let SCORE=SCORE-3
fi

echo Password Score: $SCORE
