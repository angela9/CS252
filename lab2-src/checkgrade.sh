#!/bin/bash

> my_scores
> goal_scores
score=0
goal=("Error: Password length invalid." "Error: Password length invalid." "Password Score: 11" "Password Score: 36" "Password Score: 16" "Password Score: 41" "Password Score: 17" "Password Score: 17" "Password Score: 3" "Password Score: 3" "Password Score: 17" "Password Score: 14" "Password Score: 17" "Password Score: 14" "Password Score: 8" "Password Score: 32" "Password Score: 26" "Password Score: 21")
passwords=(qqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqqq abcd5 aBcDeF aBcDeFgHiJkLmNoPqRsTuVwXyZaBcDe aBc9F1 aBcDeFg94JkLmNo8qRsTuVwXyZaBcD2 aBcDeF@ aBcDeF$ AbCdEfGG aBcDeFgg aBcDeFgHiJkawfa abcdefghijkl AbCdEfGhIjKAWFA ABCDEFGHIJKL 123456 ab12345f2456gD986f2e35a6f Exon#Mobi@Le21 123456789abcdef@gDWSS@Aw4)
for ((i=0;i<${#goal[@]};++i)); do
    echo ${passwords[i]} > out.txt
    $(./pwcheck.sh out.txt > my_scores)
    echo ${goal[i]} > goal_scores
    diff my_scores goal_scores
    if [ $? -eq 0 ]; then
        score=$((score+1))
    else
        echo "Error with password:"
        echo ${passwords[i]}
    fi
done
echo "Testcases passed:"
echo $score
