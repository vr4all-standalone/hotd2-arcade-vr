package com.flycast.emulator;

import org.junit.*;
import org.junit.rules.TemporaryFolder;
import static org.junit.Assert.*;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.zip.*;

/** Synthetic CRC-matched stand-ins only; never reads or stores game data. */
public class Hotd2ArcadeFilesTest {
    @Rule public TemporaryFolder tmp = new TemporaryFolder();
    static final String[] NAMES = {"epr-21585.ic22","mpr-21386.ic1","mpr-21387.ic2","mpr-21388.ic3","mpr-21389.ic4","mpr-21390.ic5","mpr-21391.ic6","mpr-21392.ic7","mpr-21393.ic8","mpr-21394.ic9","mpr-21395.ic10","mpr-21396.ic11","mpr-21397.ic12s","mpr-21398.ic13s","mpr-21399.ic14s","mpr-21400.ic15s","mpr-21401.ic16s","mpr-21402.ic17s","mpr-21403.ic18s","mpr-21404.ic19s","mpr-21405.ic20s"};
    static final long[] CRCS = {0xb23d1a0cL,0x88fb0562L,0x5f4dd576L,0x3e62fca4L,0x6f73a852L,0xc7950445L,0x5a812247L,0x17e9414aL,0x5d2d8134L,0xeacaf26dL,0x1e3686beL,0x5ada00a2L,0x9eff6247L,0x8a80b16aL,0x7ae20dafL,0xfbb8641bL,0x3881ec23L,0x66bff6e4L,0x8cd2f654L,0x6cf6e705L,0x495e6265L};
    static final Map<String,byte[]> DATA = new LinkedHashMap<>();
    @AfterClass public static void releaseFixtures(){ DATA.clear(); }
    @BeforeClass public static void fixtures() {
        for(int i=0;i<NAMES.length;i++) DATA.put(NAMES[i], forge(i==0?0x200000:0x800000,CRCS[i]));
        DATA.put("epr-21330.ic27", forge(0x200000,0x9e3bfa1bL));
    }
    // Solve the 32 CRC influence bits of the final four bytes over GF(2).
    static byte[] forge(int size,long wanted) {
        byte[] bytes=new byte[size]; CRC32 crc=new CRC32(); crc.update(bytes); int base=(int)crc.getValue();
        int[] values=new int[32], masks=new int[32];
        for(int bit=0;bit<32;bit++) {
            bytes[size-4+bit/8]=(byte)(1<<(bit%8)); crc.reset(); crc.update(bytes); int value=(int)crc.getValue()^base, mask=1<<bit;
            bytes[size-4+bit/8]=0;
            for(int b=31;b>=0;b--) if((value&(1<<b))!=0) {
                if(values[b]==0) {values[b]=value;masks[b]=mask;break;} value^=values[b];mask^=masks[b];
            }
        }
        int value=(int)wanted^base, mask=0;
        for(int b=31;b>=0;b--) if((value&(1<<b))!=0) {value^=values[b];mask^=masks[b];}
        for(int bit=0;bit<32;bit++) if((mask&(1<<bit))!=0) bytes[size-4+bit/8]|=(byte)(1<<(bit%8));
        crc.reset();crc.update(bytes);assertEquals(wanted,crc.getValue());return bytes;
    }
    File zip(File root,String name,Map<String,byte[]> entries)throws Exception {
        File file=new File(root,name); root.mkdirs();
        try(ZipOutputStream out=new ZipOutputStream(new FileOutputStream(file))) {
            out.setLevel(1);
            for(Map.Entry<String,byte[]> e:entries.entrySet()) {out.putNextEntry(new ZipEntry(e.getKey()));out.write(e.getValue());out.closeEntry();}
        }return file;
    }
    Map<String,byte[]> game() {Map<String,byte[]> files=new LinkedHashMap<>(DATA);files.remove("epr-21330.ic27");return files;}
    File complete()throws Exception {File dir=tmp.newFolder();zip(dir,"hotd2.zip",game());bios(dir);return dir;}
    void bios(File dir)throws Exception {zip(dir,"hod2bios.zip",Collections.singletonMap("epr-21330.ic27",DATA.get("epr-21330.ic27")));}
    Hotd2ArcadeFiles.Result find(File dir){return Hotd2ArcadeFiles.find(Collections.singletonList(dir));}
    @Test public void completeUsaArcadeCartridgeNeedsNoDisc()throws Exception {File dir=complete();assertTrue(find(dir).missing.toString(),find(dir).missing.isEmpty());assertNotNull(Hotd2ArcadeFiles.launchPath(null,Collections.singletonList(dir)));}
    @Test public void allTwentyOneChipsRequired()throws Exception {
        File dir=tmp.newFolder();bios(dir);
        for(String name:NAMES) {
            Map<String,byte[]> f=game();f.remove(name);zip(dir,"hotd2.zip",f);
            assertNull(name,Hotd2ArcadeFiles.launchPath(new File(dir,"hotd2.zip"),Collections.emptyList()));
        }
    }
    @Test public void dreamcastImageCannotFallbackToValidArcadeSet()throws Exception {
        File dir=complete(),disc=new File(dir,"hotd2-us.chd");Files.createFile(disc.toPath());
        assertNull(Hotd2ArcadeFiles.launchPath(disc,Collections.singletonList(dir)));
    }
    @Test public void ordinaryNaomiBiosCannotReplaceHotd2Bios()throws Exception {
        File dir=tmp.newFolder();zip(dir,"hotd2.zip",game());
        zip(dir,"naomi.zip",Collections.singletonMap("epr-21576h.ic27",forge(0x200000,0xd4895685L)));
        assertFalse(find(dir).bios);assertNull(Hotd2ArcadeFiles.launchPath(null,Collections.singletonList(dir)));
    }
    @Test public void hotd2ExportAndJapanBiosesAreAccepted()throws Exception {
        for(int region=0;region<2;region++){
            File dir=tmp.newFolder();zip(dir,"hotd2.zip",game());
            String name=region==0?"epr-21331.ic27":"epr-21329.ic27";
            long crc=region==0?0x065f8500L:0xd99e5b9bL;
            zip(dir,"hod2bios.zip",Collections.singletonMap(name,forge(0x200000,crc)));
            assertTrue(find(dir).missing.toString(),find(dir).missing.isEmpty());
        }
    }
    @Test public void exactLengthRequired()throws Exception {File dir=tmp.newFolder();Map<String,byte[]> f=game();f.put("epr-21585.ic22",new byte[16]);zip(dir,"hotd2.zip",f);bios(dir);assertFalse(find(dir).missing.isEmpty());}
    @Test public void correctlyNamedWrongProgramCrcRejected()throws Exception {File dir=tmp.newFolder();Map<String,byte[]> f=game();f.put("epr-21585.ic22",new byte[0x200000]);zip(dir,"hotd2.zip",f);bios(dir);assertFalse(find(dir).missing.isEmpty());}
    @Test public void correctlyNamedWrongSharedChipCrcRejected()throws Exception {File dir=tmp.newFolder();Map<String,byte[]> f=game();f.put("mpr-21391.ic6",new byte[0x800000]);zip(dir,"hotd2.zip",f);bios(dir);assertFalse(find(dir).missing.isEmpty());}
    @Test public void mergedOldNameExplainsCorrectCopy()throws Exception {File dir=tmp.newFolder();zip(dir,"hotd2e.zip",game());bios(dir);Hotd2ArcadeFiles.Result r=find(dir);assertFalse(r.missing.isEmpty());assertTrue(r.missing.toString(),r.missing.toString().contains("hotd2.zip"));assertNull(Hotd2ArcadeFiles.launchPath(new File(dir,"hotd2e.zip"),Collections.emptyList()));}
    @Test public void originalArcadeRevisionNotClaimed()throws Exception {File dir=tmp.newFolder();Map<String,byte[]> f=game();f.remove("epr-21585.ic22");f.put("epr-21385.ic22",forge(0x200000,0xedc20e44L));zip(dir,"hotd2o.zip",f);bios(dir);assertFalse(find(dir).missing.isEmpty());}
    @Test public void biosCanBeMergedInsideGame()throws Exception {File dir=tmp.newFolder();zip(dir,"hotd2.zip",DATA);assertTrue(find(dir).missing.toString(),find(dir).missing.isEmpty());}
    @Test public void biosMissingPreventsLaunch()throws Exception {File dir=tmp.newFolder();zip(dir,"hotd2.zip",game());assertFalse(find(dir).bios);assertNull(Hotd2ArcadeFiles.launchPath(null,Collections.singletonList(dir)));}
    @Test public void knownBiosNameWithWrongCrcRejected()throws Exception {File dir=tmp.newFolder();zip(dir,"hotd2.zip",game());zip(dir,"hod2bios.zip",Collections.singletonMap("epr-21330.ic27",new byte[0x200000]));assertFalse(find(dir).bios);}
    @Test public void knownBiosCrcWithWrongLengthRejected()throws Exception {File dir=tmp.newFolder();zip(dir,"hotd2.zip",game());zip(dir,"hod2bios.zip",Collections.singletonMap("epr-21330.ic27",forge(16,0x9e3bfa1bL)));assertFalse(find(dir).bios);}
    @Test public void corruptArchivePreventsLaunch()throws Exception {File dir=complete();File f=new File(dir,"hotd2.zip");try(RandomAccessFile out=new RandomAccessFile(f,"rw")){out.setLength(f.length()/2);}assertNull(Hotd2ArcadeFiles.launchPath(f,Collections.emptyList()));}
    @Test public void foreignExplicitLaunchDoesNotFallback()throws Exception {File dir=complete();File foreign=zip(dir,"lupinshoo.zip",game());assertNull(Hotd2ArcadeFiles.launchPath(foreign,Collections.singletonList(dir)));}
    @Test public void uppercaseNameCannotShowReadyOnCaseSensitiveStorage()throws Exception {File dir=tmp.newFolder();zip(dir,"HOTD2.ZIP",game());bios(dir);assertFalse(find(dir).missing.isEmpty());}
    @Test public void nestedCompleteSetWinsOverIncompleteSet()throws Exception {File root=tmp.newFolder();zip(root,"hotd2.zip",Collections.emptyMap());File nested=new File(root,"set");zip(nested,"hotd2.zip",game());bios(nested);assertEquals(new File(nested,"hotd2.zip").getAbsolutePath(),Hotd2ArcadeFiles.launchPath(null,Collections.singletonList(root)));}
    @Test public void traversalDepthBounded()throws Exception {File root=tmp.newFolder(),nested=new File(root,"a/b/c/d");zip(nested,"hotd2.zip",game());bios(nested);assertNull(Hotd2ArcadeFiles.launchPath(null,Collections.singletonList(root)));}
    @Test public void corruptedPayloadWithKnownMetadataRejected()throws Exception {File dir=complete();File f=new File(dir,"hotd2.zip");try(RandomAccessFile out=new RandomAccessFile(f,"rw")){out.seek(30+NAMES[0].length()+12);int b=out.read();out.seek(out.getFilePointer()-1);out.write(b^0x80);}assertNull(Hotd2ArcadeFiles.launchPath(f,Collections.emptyList()));}
    @Test public void moreThanEntryBudgetRejected()throws Exception {File dir=tmp.newFolder();Map<String,byte[]> f=new LinkedHashMap<>();for(int i=0;i<150;i++)f.put("filler"+i,new byte[0]);f.putAll(game());zip(dir,"hotd2.zip",f);bios(dir);assertFalse(find(dir).missing.isEmpty());}
    @Test public void corruptPreferredBiosCannotHideBehindValidFallback()throws Exception {
        File dir=tmp.newFolder();Map<String,byte[]> f=game();f.put("epr-21330.ic27",new byte[0x200000]);zip(dir,"hotd2.zip",f);
        zip(dir,"hod2bios.zip",Collections.singletonMap("epr-21331.ic27",forge(0x200000,0x065f8500L)));
        assertFalse("native loader selects the corrupt preferred h BIOS by name before the g fallback",find(dir).bios);
    }    @Test public void sharedTraversalVisitBudgetIsBounded()throws Exception {
        File root=tmp.newFolder(),nested=new File(root,"zz-valid-set");
        for(int i=0;i<Hotd2ArcadeFiles.WALK_LIMIT;i++) Files.createFile(new File(root,String.format(Locale.ROOT,"f%05d",i)).toPath());
        zip(nested,"hotd2.zip",game());bios(nested);
        assertNull(Hotd2ArcadeFiles.launchPath(null,Collections.singletonList(root)));
    }}