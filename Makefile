all: gradeGenCalculateDistance gradeGenEasy

transform:
	find  *.cpp scripts testdata -type f | xargs -I % sh -c 'dos2unix -n % /tmp/tmp; mv -f /tmp/tmp % || true;'

gradeGenCalculateDistance:transform
	bash scripts/grade.sh genCalculateDistance

gradeGenEasy:transform
	bash scripts/grade.sh genEasy

clean:
	rm -rf build out