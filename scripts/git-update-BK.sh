#!/bin/bash
#pull in Git updates (if no conflicts)

echo "use one or more of the following:"

#https://stackoverflow.com/questions/6335681/git-how-do-i-get-the-latest-version-of-my-code
git fetch origin
git status
#git add .
#git commit -m ‘Commit msg’
git pull
#edit files to resolve conflict
#git add .
#git commit -m ‘Fix conflicts’
#git pull

#from https://coderwall.com/p/9idt5g/keep-your-feature-branch-up-to-date
git checkout master
#git pull
#git checkout -
#git rebase master

#eof
