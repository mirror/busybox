# cp_tests.mk - Set of test cases for busybox cp
# -------------
# Copyright (C) 2000 Karl M. Hegbloom <karlheg@debian.org> GPL
#

# GNU `cp'
GCP = /bin/cp
# BusyBox `cp'
BCP = $(shell pwd)/cp

all:: cp_tests
clean:: cp_clean

cp_clean:
	- rm -rf cp_tests cp_*.{gnu,bb} cp

cp_tests: cp_clean cp
	@echo;
	@echo "No output from diff means busybox cp is functioning properly.";
	@echo "Some tests might show timestamp differences that are Ok.";

	@echo;
	${BCP} || true;

	@echo;
	mkdir cp_tests;

	# Copy a file to a copy of the file
	@echo ------------------------------;
	cd cp_tests;				\
	 echo A file > afile;			\
	 ls -l afile > ../cp_afile_afilecopy.gnu; \
	 ${GCP} afile afilecopy;		\
	 ls -l afile afilecopy >> ../cp_afile_afilecopy.gnu;

	@echo;
	rm -rf cp_tests/*;

	@echo;
	cd cp_tests;				\
	 echo A file > afile;			\
	 ls -l afile > ../cp_afile_afilecopy.bb; \
	 ${BCP} afile afilecopy;		\
	 ls -l afile afilecopy >> ../cp_afile_afilecopy.bb;

	@echo;
	@echo Might show timestamp differences.
	-diff -u cp_afile_afilecopy.gnu cp_afile_afilecopy.bb;

	@echo;
	rm -rf cp_tests/*;

	# Copy a file pointed to by a symlink
	@echo; echo ------------------------------;
	cd cp_tests;				\
	 mkdir here there;			\
	 echo A file > afile;			\
	 cd here;				\
	  ln -s ../afile .;			\

	@echo;
	cd cp_tests;				\
	 ls -lR . > ../cp_symlink.gnu;		\
	 ${GCP} here/afile there;		\
	 ls -lR . >> ../cp_symlink.gnu;

	@echo;
	rm -rf cp_tests/there/*;

	sleep 1;

	@echo;
	cd cp_tests;				\
	 ls -lR . > ../cp_symlink.bb;		\
	 ${BCP} here/afile there;		\
	 ls -lR . >> ../cp_symlink.bb;

	@echo;
	@echo Will show timestamp difference.
	-diff -u cp_symlink.gnu cp_symlink.bb;

	@echo;
	rm -rf cp_tests/*

	# Copy a symlink, useing the -a switch.
	@echo; echo ------------------------------;
	cd cp_tests;				\
	 echo A file > afile;			\
	 mkdir here there;			\
	 cd here;				\
	  ln -s ../afile .

	cd cp_test;				\
	 ls -lR . > ../cp_a_symlink.gnu;	\
	 ${GCP} -a here/afile there;		\
	 ls -lR . >> ../cp_a_symlink.gnu;

	@echo;
	rm -f cp_tests/there/*;

	sleep 1;

	@echo;
	cd cp_tests;				\
	 echo A file > afile;			\
	 ls -lR . > ../cp_a_symlink.bb;		\
	 ${BCP} -a here/afile there;		\
	 ls -lR . >> ../cp_a_symlink.bb;

	@echo;
	diff -u cp_a_symlink.gnu cp_a_symlink.bb;

	@echo;
	rm -f cp_tests/*;

	# Copy a directory into another directory with the -a switch
	@echo; echo ------------------------------;
	cd cp_tests;				\
	 mkdir here there;			\
	 echo A file > here/afile;		\
	 mkdir here/adir;			\
	 touch here/adir/afileinadir;		\
	 ln -s $$(pwd) here/alink;

	@echo;
	cd cp_tests;				\
	 ls -lR . > ../cp_a_dir_dir.gnu;	\
	 ${GCP} -a here/ there/;		\
	 ls -lR . >> ../cp_a_dir_dir.gnu;

	@echo;
	rm -rf cp_tests/there/*;

	sleep 1;

	@echo;
	cd cp_tests;				\
	 ls -lR . > ../cp_a_dir_dir.bb;		\
	 ${BCP} -a here/ there/;		\
	 ls -lR . >> ../cp_a_dir_dir.bb;

	@echo;
	diff -u cp_a_dir_dir.gnu cp_a_dir_dir.bb;

	@echo;
	rm -rf cp_tests/*;

	# Copy a set of files to a directory.
	@echo; echo ------------------------------;
	cd cp_tests;				\
	 echo A file number one > afile1;	\
	 echo A file number two, blah. > afile2; \
	 ln -s afile1 symlink1;			\
	 mkdir there;

	cd cp_tests;				\
	 ${GCP} afile1 afile2 symlink1 there/;	\
	 ls -lR > ../cp_files_dir.gnu;

	@echo;
	rm -rf cp_tests/there/*;

	@echo;
	cd cp_tests;				\
	 ${BCP} afile1 afile2 symlink1 there/;	\
	 ls -lR > ../cp_files_dir.bb;

	@echo;
	diff -u cp_files_dir.gnu cp_files_dir.bb;

	@echo;
	rm -rf cp_tests/*;

	# Copy a set of files to a directory with the -d switch.
	@echo; echo ------------------------------;
	cd cp_tests;				\
	 echo A file number one > afile1;	\
	 echo A file number two, blah. > afile2; \
	 ln -s afile1 symlink1;			\
	 mkdir there1;				\
	 ${GCP} -d afile1 afile2 symlink1 there1/; \
	 ls -lR > ../cp_d_files_dir.gnu;

	@echo;
	rm -rf cp_tests/{afile{1,2},symlink1,there1};

	@echo;
	cd cp_tests;				\
	 echo A file number one > afile1;	\
	 echo A file number two, blah. > afile2; \
	 ln -s afile1 symlink1;			\
	 mkdir there1;				\
	 ${BCP} -d afile1 afile2 symlink1 there1/; \
	 ls -lR > ../cp_d_files_dir.bb;

	@echo;
	diff -u cp_d_files_dir.gnu cp_d_files_dir.bb;

	@echo;
	rm -rf cp_tests/{afile{1,2},symlink1,there1};

	# Copy a set of files to a directory with the -p switch.
	@echo; echo ------------------------------;
	cd cp_tests;				\
	 echo A file number one > afile1;	\
	 echo A file number two, blah. > afile2; \
	 touch --date='Sat Jan 29 21:24:08 PST 2000' afile1; \
	 ln -s afile1 symlink1;			\
	 mkdir there1;				\
	 ${GCP} -p afile1 afile2 symlink1 there1/; \
	 ls -lR > ../cp_p_files_dir.gnu;

	@echo;
	rm -rf cp_tests/{afile{1,2},symlink1,there1};

	@echo;
	cd cp_tests;				\
	 echo A file number one > afile1;	\
	 echo A file number two, blah. > afile2; \
	 touch --date='Sat Jan 29 21:24:08 PST 2000' afile1; \
	 ln -s afile1 symlink1;			\
	 mkdir there1;				\
	 ${BCP} -p afile1 afile2 symlink1 there1/; \
	 ls -lR > ../cp_p_files_dir.bb;

	@echo;
	diff -u cp_p_files_dir.gnu cp_p_files_dir.bb;

	@echo;
	rm -rf cp_tests/{afile{1,2},symlink1,there1};

	# Copy a set of files to a directory with -p and -d switches.
	@echo; echo ------------------------------;
	cd cp_tests;				\
	 echo A file number one > afile1;	\
	 echo A file number two, blah. > afile2; \
	 touch --date='Sat Jan 29 21:24:08 PST 2000' afile1; \
	 ln -s afile1 symlink1;			\
	 mkdir there1;				\
	 ${GCP} -p -d afile1 afile2 symlink1 there1/; \
	 ls -lR > ../cp_pd_files_dir.gnu;

	@echo;
	rm -rf cp_tests/{afile{1,2},symlink1,there1};

	@echo;
	cd cp_tests;				\
	 echo A file number one > afile1;	\
	 echo A file number two, blah. > afile2; \
	 touch --date='Sat Jan 29 21:24:08 PST 2000' afile1; \
	 ln -s afile1 symlink1;			\
	 mkdir there1;				\
	 ${BCP} -p -d afile1 afile2 symlink1 there1/; \
	 ls -lR > ../cp_pd_files_dir.bb;

	@echo;
	diff -u cp_pd_files_dir.gnu cp_pd_files_dir.bb;

	@echo;
	rm -rf cp_tests/{afile{1,2},symlink1,there1};

	# Copy a directory into another directory with the -a switch.
	@echo; echo ------------------------------;
	cd cp_tests;				\
	 mkdir dir{a,b};			\
	 echo A file > dira/afile;		\
	 echo A file in dirb > dirb/afileindirb; \
	 ln -s dira/afile dira/alinktoafile;	\
	 mkdir dira/subdir1;			\
	 echo Another file > dira/subdir1/anotherfile; \
	 ls -lR . > ../cp_a_dira_dirb.gnu;	\
	 ${GCP} -a dira dirb;			\
	 ls -lR . >> ../cp_a_dira_dirb.gnu;

	@echo;
	rm -rf cp_tests/dir{a,b};

	@echo;
	cd cp_tests;				\
	 mkdir dir{a,b};			\
	 echo A file > dira/afile;		\
	 echo A file in dirb > dirb/afileindirb; \
	 ln -s dira/afile dira/alinktoafile;	\
	 mkdir dira/subdir1;			\
	 echo Another file > dira/subdir1/anotherfile; \
	 ls -lR . > ../cp_a_dira_dirb.bb;	\
	 ${BCP} -a dira dirb;			\
	 ls -lR . >> ../cp_a_dira_dirb.bb;

	@echo;
	diff -u cp_a_dira_dirb.gnu cp_a_dira_dirb.bb;

	@echo;
	rm -rf cp_tests/dir{a,b};

	# Copy a directory to another directory, without the -a switch.
	@echo; echo ------------------------------;
	@echo There should be an error message about cannot cp a dir to a subdir of itself.
	cd cp_tests;				\
	 touch a b c;				\
	 mkdir adir;				\
	 ls -lR . > ../cp_a_star_adir.gnu;	\
	 ${GCP} -a * adir;			\
	 ls -lR . >> ../cp_a_star_adir.gnu;

	@echo
	@echo There should be an error message about cannot cp a dir to a subdir of itself.
	cd cp_tests;				\
	 rm -rf adir;				\
	 mkdir adir;				\
	 ls -lR . > ../cp_a_star_adir.bb;	\
	 ${BCP} -a * adir;			\
	 ls -lR . >> ../cp_a_star_adir.bb;

	@echo;
	diff -u cp_a_star_adir.gnu cp_a_star_adir.bb;

	@echo;
	rm -rf cp_tests;
	@echo; echo Done.
