#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <sys/ioctl.h>
#include <linux/if_vlan.h>
#include <linux/sockios.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "busybox.h" 



int vconfig_main(int argc, char** argv) {
   int fd;
   struct vlan_ioctl_args if_request;
   
   char* cmd = NULL;
   char* if_name = NULL;
   unsigned int vid = 0;
   unsigned int skb_priority;
   unsigned short vlan_qos;
   unsigned int nm_type = VLAN_NAME_TYPE_PLUS_VID;

   char* conf_file_name = "/proc/net/vlan/config";

   memset(&if_request, 0, sizeof(struct vlan_ioctl_args));
   
   if ((argc < 3) || (argc > 5)) {
      error_msg_and_die("Expecting argc to be 3-5, inclusive.  Was: %d\n",argc);
   }
   else {
      cmd = argv[1];
          
      if (strcasecmp(cmd, "set_name_type") == 0) {
         if (strcasecmp(argv[2], "VLAN_PLUS_VID") == 0) {
            nm_type = VLAN_NAME_TYPE_PLUS_VID;
         }
         else if (strcasecmp(argv[2], "VLAN_PLUS_VID_NO_PAD") == 0) {
            nm_type = VLAN_NAME_TYPE_PLUS_VID_NO_PAD;
         }
         else if (strcasecmp(argv[2], "DEV_PLUS_VID") == 0) {
            nm_type = VLAN_NAME_TYPE_RAW_PLUS_VID;
         }
         else if (strcasecmp(argv[2], "DEV_PLUS_VID_NO_PAD") == 0) {
            nm_type = VLAN_NAME_TYPE_RAW_PLUS_VID_NO_PAD;
         }
         else {
            error_msg_and_die("Invalid name type.\n");
         }
         if_request.u.name_type = nm_type;
      }
      else {
         if_name = argv[2];
         if (strlen(if_name) > 15) {
            error_msg_and_die("ERROR:  if_name must be 15 characters or less.\n");
         }
         strcpy(if_request.device1, if_name);
      }

      if (argc == 4) {
         vid = atoi(argv[3]);
         if_request.u.VID = vid;
      }

      if (argc == 5) {
         skb_priority = atoi(argv[3]);
         vlan_qos = atoi(argv[4]);
         if_request.u.skb_priority = skb_priority;
         if_request.vlan_qos = vlan_qos;
      }
   }
   
   // Open up the /proc/vlan/config
   if ((fd = open(conf_file_name, O_RDONLY)) < 0) {
      error_msg("WARNING:  Could not open /proc/net/vlan/config.  Maybe you need to load the 8021q module, or maybe you are not using PROCFS??\n");
   }
   else {
      close(fd);
   }

   /* We use sockets now, instead of the file descriptor */
   if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      error_msg_and_die("FATAL:  Couldn't open a socket..go figure!\n");
   }   

   /* add */
   if (strcasecmp(cmd, "add") == 0) {
      if_request.cmd = ADD_VLAN_CMD;
      if (ioctl(fd, SIOCSIFVLAN, &if_request) < 0) {
         error_msg("ERROR: trying to add VLAN #%u to IF -:%s:-  error: %s\n",
                    vid, if_name, strerror(errno));                 
      }
      else {
         printf("Added VLAN with VID == %u to IF -:%s:-\n", vid, if_name);
         if (vid == 1) {
            error_msg("WARNING:  VLAN 1 does not work with many switches,\nconsider another number if you have problems.\n");
         }
      }
   }//if
   else if (strcasecmp(cmd, "rem") == 0) {
      if_request.cmd = DEL_VLAN_CMD;
      if (ioctl(fd, SIOCSIFVLAN, &if_request) < 0) {
         error_msg("ERROR: trying to remove VLAN -:%s:- error: %s\n",
                 if_name, strerror(errno));         
      }
      else {
         printf("Removed VLAN -:%s:-\n", if_name);
      }
   }//if
   else if (strcasecmp(cmd, "set_egress_map") == 0) {
      if_request.cmd = SET_VLAN_EGRESS_PRIORITY_CMD;
      if (ioctl(fd, SIOCSIFVLAN, &if_request) < 0) {
         error_msg("ERROR: trying to set egress map on device -:%s:- error: %s\n",
                 if_name, strerror(errno));         
      }
      else {
         printf("Set egress mapping on device -:%s:- "
                 "Should be visible in /proc/net/vlan/%s\n",
                 if_name, if_name);
      }
   }
   else if (strcasecmp(cmd, "set_ingress_map") == 0) {
      if_request.cmd = SET_VLAN_INGRESS_PRIORITY_CMD;
      if (ioctl(fd, SIOCSIFVLAN, &if_request) < 0) {
         error_msg("ERROR: trying to set ingress map on device -:%s:- error: %s\n",
                 if_name, strerror(errno));
      }
      else {
         printf("Set ingress mapping on device -:%s:- "
                 "Should be visible in /proc/net/vlan/%s\n",
                 if_name, if_name);                
      }
   }   
   else if (strcasecmp(cmd, "set_flag") == 0) {
      if_request.cmd = SET_VLAN_FLAG_CMD;
      if (ioctl(fd, SIOCSIFVLAN, &if_request) < 0) {
         error_msg("ERROR: trying to set flag on device -:%s:- error: %s\n",
                 if_name, strerror(errno));
      }
      else {
         printf("Set flag on device -:%s:- "
                 "Should be visible in /proc/net/vlan/%s\n",
                 if_name, if_name);
      }
   }
   else if (strcasecmp(cmd, "set_name_type") == 0) {
      if_request.cmd = SET_VLAN_NAME_TYPE_CMD;
      if (ioctl(fd, SIOCSIFVLAN, &if_request) < 0) {
         error_msg("ERROR: trying to set name type for VLAN subsystem, error: %s\n",
                 strerror(errno));         
      }
      else {
         printf("Set name-type for VLAN subsystem."
                 " Should be visible in /proc/net/vlan/config\n");         
      }
   }
   else {
      error_msg_and_die("Unknown command -:%s:-\n", cmd);
   }

   return 0;
}/* main */
