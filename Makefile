all: gradeGenCalculateDistance gradeGenEasy

gradeGenCalculateDistance:
	bash scripts/grade.sh genCalculateDistance

gradeGenEasy:
	bash scripts/grade.sh genEasy

clean:
	rm -rf build out
