#include "internal.h"

extern int
post_process(const struct FileInfo * i)
{
	int	status = 0;

	if ( i->destination == 0 || *i->destination == 0 )
		return 0;

	if ( status == 0 && i->changeMode ) {
		mode_t	mode = i->stat.st_mode & 07777;
		mode &= i->andWithMode;
		mode |= i->orWithMode;
		status = chmod(i->destination, mode);

		if ( status != 0 && i->complainInPostProcess && !i->force ) {
			name_and_error(i->destination);
			return 1;
		}
	}

	if ( i->changeUserID || i->changeGroupID ) {
		uid_t	uid = i->stat.st_uid;
		gid_t	gid = i->stat.st_gid;

		if ( i->changeUserID )
			uid = i->userID;
		if ( i->changeGroupID )
			gid = i->groupID;

		status = chown(i->destination, uid, gid);

		if ( status != 0 && i->complainInPostProcess && !i->force ) {
			name_and_error(i->destination);
			return 1;
		}
	}

	return status;
}
