// SPDX-License-Identifier: GPL-2.0-or-later
package com.flycast.emulator;

import java.io.*;
import java.util.*;
import java.util.zip.*;

/** Verified USA arcade cartridge set. Checks all required chip lengths and streamed CRCs.
 * This app needs no CHD. Plain Java so the setup and explicit-launch policy share JVM tests. */
final class Hotd2ArcadeFiles {
    private Hotd2ArcadeFiles() {}
    static final String ZIP = "hotd2.zip", BIOS_ZIP = "hod2bios.zip";
    static final int WALK_LIMIT = 5000;
    private static final int DEPTH = 3, ZIP_ENTRY_LIMIT = 128;
    private static final String[] CHIPS = {"epr-21585.ic22","mpr-21386.ic1","mpr-21387.ic2","mpr-21388.ic3","mpr-21389.ic4","mpr-21390.ic5","mpr-21391.ic6","mpr-21392.ic7","mpr-21393.ic8","mpr-21394.ic9","mpr-21395.ic10","mpr-21396.ic11","mpr-21397.ic12s","mpr-21398.ic13s","mpr-21399.ic14s","mpr-21400.ic15s","mpr-21401.ic16s","mpr-21402.ic17s","mpr-21403.ic18s","mpr-21404.ic19s","mpr-21405.ic20s"};
    private static final long[] CRCS = {0xb23d1a0cL,0x88fb0562L,0x5f4dd576L,0x3e62fca4L,0x6f73a852L,0xc7950445L,0x5a812247L,0x17e9414aL,0x5d2d8134L,0xeacaf26dL,0x1e3686beL,0x5ada00a2L,0x9eff6247L,0x8a80b16aL,0x7ae20dafL,0xfbb8641bL,0x3881ec23L,0x66bff6e4L,0x8cd2f654L,0x6cf6e705L,0x495e6265L};
    // The three 2 MiB HOTD2-specific BIOSes supported by the native loader.
    private static final String[] BIOS_NAMES = {"epr-21331.ic27","epr-21330.ic27","epr-21329.ic27"};
    private static final long[] BIOS_CRCS = {0x065f8500L,0x9e3bfa1bL,0xd99e5b9bL};
    static final class Result {
        File zip;
        boolean bios;
        final List<String> missing = new ArrayList<>();
    }
    static Result find(List<File> roots) {
        List<File[]> candidates = new ArrayList<>();
        for(File root:roots) if(root!=null) search(root,root,DEPTH,candidates,new int[]{WALK_LIMIT});
        Result best=null;
        for(File[] candidate:candidates) {
            Result r=check(candidate[0],candidate[1]);
            if(best==null || r.missing.size()<best.missing.size()) best=r;
            if(r.missing.isEmpty()) break;
        }
        if(best!=null) return best;
        Result r=new Result();
        for(File root:roots) if(root!=null && biosState(new File(root,BIOS_ZIP))==1) {r.bios=true;break;}
        r.missing.add(ZIP+": the complete The House of the Dead 2 USA arcade MAME cartridge set, in Download. No CHD is needed.");
        if(!r.bios) r.missing.add(BIOS_ZIP+": a supported HOTD2-specific BIOS beside "+ZIP+", or its BIOS file inside the game ZIP");
        return r;
    }
    static String launchPath(File requested,List<File> roots) {
        Result r;
        if(requested==null) r=find(roots);
        else {
            requested=requested.getAbsoluteFile();
            if(!requested.getName().equals(ZIP) || !requested.isFile()) return null;
            r=check(requested,requested.getParentFile());
        }
        return r.zip!=null && r.missing.isEmpty()?r.zip.getAbsolutePath():null;
    }
    private static void search(File dir,File root,int depth,List<File[]> out,int[] left) {
        if(left[0]<=0) return;
        File[] files=dir.listFiles();if(files==null)return;Arrays.sort(files);
        int count=Math.min(left[0],files.length);left[0]-=count;
        for(int i=0;i<count;i++) {
            String name=files[i].getName();
            if((name.equalsIgnoreCase(ZIP)||name.equalsIgnoreCase("hotd2o.zip")||name.equalsIgnoreCase("hotd2e.zip")||name.equalsIgnoreCase("hotd2p.zip"))&&files[i].isFile()) out.add(new File[]{files[i],root});
        }
        if(depth>0) for(int i=0;i<count&&left[0]>0;i++) if(!files[i].getName().startsWith(".")&&files[i].isDirectory()) search(files[i],root,depth-1,out,left);
    }
    private static Result check(File found,File root) {
        Result r=new Result();r.zip=found;
        String where=found.getParentFile().getAbsolutePath();
        if(!found.getName().equals(ZIP)) {
            if(found.getName().equalsIgnoreCase(ZIP))
                r.missing.add(ZIP+": rename "+found.getName()+" using lowercase letters (in "+where+")");
            else
                r.missing.add(ZIP+": only the USA arcade hotd2 cartridge is verified. Other revisions and Dreamcast images are unsupported; renaming does not convert them.");
        } else {
            String error=gameError(found);
            if(error!=null) r.missing.add(ZIP+": "+error+" (in "+where+")");
        }
        int mergedBios=biosState(found), besideBios=biosState(new File(found.getParentFile(),BIOS_ZIP));
        r.bios=mergedBios>=0 && besideBios>=0 && (mergedBios==1 || besideBios==1);
        if(!r.bios)r.missing.add(BIOS_ZIP+": a supported HOTD2-specific BIOS beside "+ZIP+" (in "+where+"), or its BIOS file inside the game ZIP");
        return r;
    }
    private static List<? extends ZipEntry> entries(ZipFile zip) throws IOException {
        if(zip.size()>ZIP_ENTRY_LIMIT) throw new IOException("too many ZIP entries (limit "+ZIP_ENTRY_LIMIT+")");
        return Collections.list(zip.entries());
    }
    private static String gameError(File archive) {
        try(ZipFile zip=new ZipFile(archive)) {
            List<? extends ZipEntry> entries=entries(zip);
            for(int i=0;i<CHIPS.length;i++) {
                long length=i==0?0x200000:0x800000;ZipEntry match=null;
                // Match the native loader's CRC lookup, including renamed chip files.
                for(ZipEntry e:entries) if(!e.isDirectory()&&e.getCrc()==CRCS[i]&&e.getSize()==length) {match=e;break;}
                if(match==null) return "needs "+CHIPS[i]+" ("+length+" bytes, CRC "+String.format(Locale.ROOT,"%08x",CRCS[i])+") in the complete USA arcade set";
                if(!validPayload(zip,match,length,CRCS[i])) return "corrupt or truncated chip "+CHIPS[i];
            }
            return null;
        } catch(IOException|RuntimeException e) {return "unreadable or invalid ZIP; use a complete USA arcade cartridge archive";}
    }
    /** -1 means a corrupt known BIOS that the native filename fallback could select;
     * 0 means absent; 1 means a verified BIOS. Corrupt preferred files cannot hide behind
     * another valid revision in the game or beside archive. */
    private static int biosState(File archive) {
        try(ZipFile zip=new ZipFile(archive)) {
            boolean usable=false;
            for(ZipEntry entry:entries(zip)) {
                if(entry.isDirectory()) continue;
                boolean verified=false;
                for(int i=0;i<BIOS_NAMES.length;i++) if(entry.getName().equals(BIOS_NAMES[i])) {
                    if(entry.getSize()!=0x200000 || entry.getCrc()!=BIOS_CRCS[i]
                            || !validPayload(zip,entry,0x200000,BIOS_CRCS[i])) return -1;
                    usable=true;verified=true;break;
                }
                if(!verified && entry.getSize()==0x200000)
                    for(long crc:BIOS_CRCS) if(entry.getCrc()==crc && validPayload(zip,entry,0x200000,crc)) {usable=true;break;}
            }
            return usable?1:0;
        } catch(IOException|RuntimeException e) {return archive.isFile()?-1:0;}
    }

    private static boolean validPayload(ZipFile zip,ZipEntry entry,long length,long crc) throws IOException {
        CRC32 actual=new CRC32();byte[] buffer=new byte[65536];long total=0;
        try(InputStream in=zip.getInputStream(entry)) {
            for(int n;(n=in.read(buffer))!=-1;) {
                total+=n;if(total>length)return false;actual.update(buffer,0,n);
            }
        }
        return total==length&&actual.getValue()==crc;
    }
}