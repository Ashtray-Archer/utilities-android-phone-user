.class public Lacceptance/RecipeFileObserver;
.super Ljava/lang/Object;
.field private static final seed:Ljava/lang/String; = "True|22:green-grocer-recipe-v12:r14:Soup1:21:01:0"
.field private static final payload:Ljava/lang/String; = "22:green-grocer-recipe-v12:r18:Saved λ🦉1:31:18:rice-0014:Rice4:mass20:184467440737095516171:318:rinse:\nthen simmer1:17:lentils1:144:https://www.youtube.com/watch?v=fixture#t=809:食事 🦉\n\u0000end"

.method private static equal(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    .registers 4
    invoke-virtual {p0, p1}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v0
    if-nez v0, :ok
    sget-object v0, Ljava/lang/System;->err:Ljava/io/PrintStream;
    invoke-virtual {v0, p2}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    const/16 v0, 83
    invoke-static {v0}, Ljava/lang/System;->exit(I)V
  :ok
    return-void
.end method

.method private static suffix(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
    .registers 3
    invoke-virtual {p0, p1}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    return-object v0
.end method

.method private static accepted()Ljava/lang/String;
    .registers 2
    const-string v0, "False|"
    sget-object v1, Lacceptance/RecipeFileObserver;->payload:Ljava/lang/String;
    invoke-virtual {v0, v1}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    return-object v0
.end method

.method private static bytes(Ljava/lang/String;)[B
    .registers 7
    new-instance v0, Ljava/io/FileInputStream;
    invoke-direct {v0, p0}, Ljava/io/FileInputStream;-><init>(Ljava/lang/String;)V
    new-instance v1, Ljava/io/ByteArrayOutputStream;
    invoke-direct {v1}, Ljava/io/ByteArrayOutputStream;-><init>()V
    const/16 v2, 4096
    new-array v2, v2, [B
  :read
    invoke-virtual {v0, v2}, Ljava/io/FileInputStream;->read([B)I
    move-result v3
    if-ltz v3, :done
    const/4 v4, 0
    invoke-virtual {v1, v2, v4, v3}, Ljava/io/ByteArrayOutputStream;->write([BII)V
    goto :read
  :done
    invoke-virtual {v0}, Ljava/io/FileInputStream;->close()V
    invoke-virtual {v1}, Ljava/io/ByteArrayOutputStream;->toByteArray()[B
    move-result-object v0
    return-object v0
.end method

.method private static write_bytes(Ljava/lang/String;[B)V
    .registers 3
    new-instance v0, Ljava/io/FileOutputStream;
    invoke-direct {v0, p0}, Ljava/io/FileOutputStream;-><init>(Ljava/lang/String;)V
    invoke-virtual {v0, p1}, Ljava/io/FileOutputStream;->write([B)V
    invoke-virtual {v0}, Ljava/io/FileOutputStream;->close()V
    return-void
.end method

.method private static disk_equals(Ljava/lang/String;Ljava/lang/String;)V
    .registers 5
    invoke-static {p0}, Lacceptance/RecipeFileObserver;->bytes(Ljava/lang/String;)[B
    move-result-object v0
    const-string v1, "UTF-8"
    invoke-virtual {p1, v1}, Ljava/lang/String;->getBytes(Ljava/lang/String;)[B
    move-result-object v1
    invoke-static {v0, v1}, Ljava/util/Arrays;->equals([B[B)Z
    move-result v0
    if-nez v0, :ok
    const-string v0, "expected"
    const-string v1, "different"
    const-string v2, "FAIL exact standard UTF-8 file bytes"
    invoke-static {v0, v1, v2}, Lacceptance/RecipeFileObserver;->equal(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
  :ok
    return-void
.end method

.method private static write(Ljava/lang/String;)V
    .registers 5
    sget-object v0, Lacceptance/RecipeFileObserver;->payload:Ljava/lang/String;
    invoke-static {p0, v0}, LIdric/Generated;->store_recipe_file(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    invoke-static {}, Lacceptance/RecipeFileObserver;->accepted()Ljava/lang/String;
    move-result-object v2
    const-string v3, "FAIL durable save uses canonical recipe transition"
    invoke-static {v2, v1, v3}, Lacceptance/RecipeFileObserver;->equal(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    invoke-static {p0, v0}, Lacceptance/RecipeFileObserver;->disk_equals(Ljava/lang/String;Ljava/lang/String;)V
    return-void
.end method

.method private static reopen(Ljava/lang/String;)V
    .registers 5
    invoke-static {p0}, LIdric/Generated;->restore_recipe_file(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    invoke-static {}, Lacceptance/RecipeFileObserver;->accepted()Ljava/lang/String;
    move-result-object v1
    const-string v2, "FAIL fresh process restores exact recipe and revokes review"
    invoke-static {v1, v0, v2}, Lacceptance/RecipeFileObserver;->equal(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    sget-object v0, Lacceptance/RecipeFileObserver;->payload:Ljava/lang/String;
    invoke-static {p0, v0}, Lacceptance/RecipeFileObserver;->disk_equals(Ljava/lang/String;Ljava/lang/String;)V
    return-void
.end method

.method private static refusals(Ljava/lang/String;)V
    .registers 9
    const-string v0, ".bad"
    invoke-static {p0, v0}, Lacceptance/RecipeFileObserver;->suffix(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    sget-object v1, Lacceptance/RecipeFileObserver;->payload:Ljava/lang/String;
    const-string v2, "UTF-8"
    invoke-virtual {v1, v2}, Ljava/lang/String;->getBytes(Ljava/lang/String;)[B
    move-result-object v2
    # Everything before the name is ASCII: this index is also its byte offset.
    const-string v3, "Saved"
    invoke-virtual {v1, v3}, Ljava/lang/String;->indexOf(Ljava/lang/String;)I
    move-result v3
    const/4 v4, -1
    aput-byte v4, v2, v3
    invoke-static {v0, v2}, Lacceptance/RecipeFileObserver;->write_bytes(Ljava/lang/String;[B)V
    invoke-static {v0}, LIdric/Generated;->restore_recipe_file(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v3
    sget-object v4, Lacceptance/RecipeFileObserver;->seed:Ljava/lang/String;
    const-string v5, "FAIL invalid UTF-8 preserves recipes and review"
    invoke-static {v4, v3, v5}, Lacceptance/RecipeFileObserver;->equal(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const-string v3, "3:bad"
    const-string v5, "UTF-8"
    invoke-virtual {v3, v5}, Ljava/lang/String;->getBytes(Ljava/lang/String;)[B
    move-result-object v3
    invoke-static {v0, v3}, Lacceptance/RecipeFileObserver;->write_bytes(Ljava/lang/String;[B)V
    invoke-static {v0}, LIdric/Generated;->restore_recipe_file(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v3
    const-string v5, "FAIL corrupt file preserves recipes and review"
    invoke-static {v4, v3, v5}, Lacceptance/RecipeFileObserver;->equal(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const-string v3, ".missing"
    invoke-static {p0, v3}, Lacceptance/RecipeFileObserver;->suffix(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
    move-result-object v3
    invoke-static {v3}, LIdric/Generated;->restore_recipe_file(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v3
    const-string v5, "FAIL missing file preserves recipes and review"
    invoke-static {v4, v3, v5}, Lacceptance/RecipeFileObserver;->equal(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const-string v3, "relative.gg"
    invoke-static {v3, v1}, LIdric/Generated;->store_recipe_file(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
    move-result-object v3
    const-string v5, "FAIL relative path refusal"
    invoke-static {v4, v3, v5}, Lacceptance/RecipeFileObserver;->equal(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const-string v3, "3:bad"
    invoke-static {p0, v3}, LIdric/Generated;->store_recipe_file(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
    move-result-object v3
    const-string v5, "FAIL invalid save preserves recipes and review"
    invoke-static {v4, v3, v5}, Lacceptance/RecipeFileObserver;->equal(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const-string v3, "/child"
    invoke-static {p0, v3}, Lacceptance/RecipeFileObserver;->suffix(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
    move-result-object v3
    invoke-static {v3, v1}, LIdric/Generated;->store_recipe_file(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
    move-result-object v3
    const-string v5, "FAIL failed write preserves recipes and review"
    invoke-static {v4, v3, v5}, Lacceptance/RecipeFileObserver;->equal(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    invoke-static {p0, v1}, Lacceptance/RecipeFileObserver;->disk_equals(Ljava/lang/String;Ljava/lang/String;)V
    return-void
.end method

.method private static interrupt(Ljava/lang/String;)V
    .registers 5
    new-instance v0, Ljava/io/File;
    invoke-direct {v0, p0}, Ljava/io/File;-><init>(Ljava/lang/String;)V
    new-instance v1, Landroid/util/AtomicFile;
    invoke-direct {v1, v0}, Landroid/util/AtomicFile;-><init>(Ljava/io/File;)V
    invoke-virtual {v1}, Landroid/util/AtomicFile;->startWrite()Ljava/io/FileOutputStream;
    move-result-object v0
    const-string v1, "3:bad"
    const-string v2, "UTF-8"
    invoke-virtual {v1, v2}, Ljava/lang/String;->getBytes(Ljava/lang/String;)[B
    move-result-object v1
    invoke-virtual {v0, v1}, Ljava/io/FileOutputStream;->write([B)V
    invoke-virtual {v0}, Ljava/io/FileOutputStream;->getFD()Ljava/io/FileDescriptor;
    move-result-object v0
    invoke-virtual {v0}, Ljava/io/FileDescriptor;->sync()V
    # Leave the real AtomicFile write unfinished and exit this VM.
    const/4 v0, 0
    invoke-static {v0}, Ljava/lang/System;->exit(I)V
    return-void
.end method

.method private static limits(Ljava/lang/String;)V
    .registers 7
    const-string v0, ".large"
    invoke-static {p0, v0}, Lacceptance/RecipeFileObserver;->suffix(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    new-instance v1, Ljava/io/RandomAccessFile;
    const-string v2, "rw"
    invoke-direct {v1, v0, v2}, Ljava/io/RandomAccessFile;-><init>(Ljava/lang/String;Ljava/lang/String;)V
    const-wide/32 v2, 1048577
    invoke-virtual {v1, v2, v3}, Ljava/io/RandomAccessFile;->setLength(J)V
    invoke-virtual {v1}, Ljava/io/RandomAccessFile;->close()V
  :large_start
    invoke-static {v0}, LIdric/Generated;->read_recipe_file_text(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
  :large_end
    const-string v1, "no exception"
    goto :large_checked
    .catch Ljava/lang/IllegalArgumentException; {:large_start .. :large_end} :large_refused
  :large_refused
    move-exception v1
    invoke-virtual {v1}, Ljava/lang/Throwable;->getMessage()Ljava/lang/String;
    move-result-object v1
  :large_checked
    const-string v2, "Recipe file exceeds byte budget"
    const-string v3, "FAIL file byte budget before decoding"
    invoke-static {v2, v1, v3}, Lacceptance/RecipeFileObserver;->equal(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
  :unicode_start
    const-string v0, "\ud800"
    invoke-static {p0, v0}, LIdric/Generated;->write_recipe_file_text(Ljava/lang/String;Ljava/lang/String;)V
  :unicode_end
    const-string v1, "no exception"
    goto :unicode_checked
    .catch Ljava/lang/IllegalArgumentException; {:unicode_start .. :unicode_end} :unicode_refused
  :unicode_refused
    move-exception v1
    invoke-virtual {v1}, Ljava/lang/Throwable;->getMessage()Ljava/lang/String;
    move-result-object v1
  :unicode_checked
    const-string v2, "Recipe text is not valid Unicode"
    const-string v3, "FAIL malformed Unicode refuses write"
    invoke-static {v2, v1, v3}, Lacceptance/RecipeFileObserver;->equal(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    const v0, 262145
    new-array v0, v0, [C
    new-instance v1, Ljava/lang/String;
    invoke-direct {v1, v0}, Ljava/lang/String;-><init>([C)V
  :text_start
    invoke-static {p0, v1}, LIdric/Generated;->write_recipe_file_text(Ljava/lang/String;Ljava/lang/String;)V
  :text_end
    const-string v1, "no exception"
    goto :text_checked
    .catch Ljava/lang/IllegalArgumentException; {:text_start .. :text_end} :text_refused
  :text_refused
    move-exception v1
    invoke-virtual {v1}, Ljava/lang/Throwable;->getMessage()Ljava/lang/String;
    move-result-object v1
  :text_checked
    const-string v2, "Recipe file exceeds text budget"
    const-string v3, "FAIL file text budget before replacement"
    invoke-static {v2, v1, v3}, Lacceptance/RecipeFileObserver;->equal(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    sget-object v0, Lacceptance/RecipeFileObserver;->payload:Ljava/lang/String;
    invoke-static {p0, v0}, Lacceptance/RecipeFileObserver;->disk_equals(Ljava/lang/String;Ljava/lang/String;)V
    return-void
.end method

.method private static long_text(Ljava/lang/String;)V
    .registers 7
    const-string v0, ".long"
    invoke-static {p0, v0}, Lacceptance/RecipeFileObserver;->suffix(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;
    move-result-object v0
    # Seven UTF-16 prefix units plus this body reach the accepted limit exactly.
    const v1, 262137
    new-array v1, v1, [C
    const/16 v2, 120
    invoke-static {v1, v2}, Ljava/util/Arrays;->fill([CC)V
    new-instance v2, Ljava/lang/String;
    invoke-direct {v2, v1}, Ljava/lang/String;-><init>([C)V
    const-string v1, " \nλ🦉\u0000\n"
    invoke-virtual {v1, v2}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    invoke-static {v0, v1}, LIdric/Generated;->write_recipe_file_text(Ljava/lang/String;Ljava/lang/String;)V
    invoke-static {v0}, LIdric/Generated;->read_recipe_file_text(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v2
    const-string v3, "FAIL accepted text budget round trip preserves exact Unicode"
    invoke-static {v1, v2, v3}, Lacceptance/RecipeFileObserver;->equal(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V
    invoke-static {v0, v1}, Lacceptance/RecipeFileObserver;->disk_equals(Ljava/lang/String;Ljava/lang/String;)V
    return-void
.end method

.method public static main([Ljava/lang/String;)V
    .registers 7
    sget-object v4, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v5, "file_process_pid\t"
    invoke-virtual {v4, v5}, Ljava/io/PrintStream;->print(Ljava/lang/String;)V
    invoke-static {}, Landroid/os/Process;->myPid()I
    move-result v5
    invoke-virtual {v4, v5}, Ljava/io/PrintStream;->println(I)V
    const/4 v0, 0
    aget-object v1, p0, v0
    const/4 v0, 1
    aget-object v2, p0, v0
    const-string v0, "write"
    invoke-virtual {v0, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v0
    if-eqz v0, :read
    invoke-static {v1}, Lacceptance/RecipeFileObserver;->write(Ljava/lang/String;)V
    goto :pass
  :read
    const-string v0, "reopen"
    invoke-virtual {v0, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v0
    if-eqz v0, :refusals
    invoke-static {v1}, Lacceptance/RecipeFileObserver;->reopen(Ljava/lang/String;)V
    goto :pass
  :refusals
    const-string v0, "refusals"
    invoke-virtual {v0, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v0
    if-eqz v0, :interrupt
    invoke-static {v1}, Lacceptance/RecipeFileObserver;->refusals(Ljava/lang/String;)V
    goto :pass
  :interrupt
    const-string v0, "interrupt"
    invoke-virtual {v0, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v0
    if-eqz v0, :limits
    invoke-static {v1}, Lacceptance/RecipeFileObserver;->interrupt(Ljava/lang/String;)V
    goto :pass
  :limits
    const-string v0, "limits"
    invoke-virtual {v0, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v0
    if-eqz v0, :long
    invoke-static {v1}, Lacceptance/RecipeFileObserver;->limits(Ljava/lang/String;)V
    goto :pass
  :long
    const-string v0, "long"
    invoke-virtual {v0, v2}, Ljava/lang/String;->equals(Ljava/lang/Object;)Z
    move-result v0
    if-eqz v0, :unknown
    invoke-static {v1}, Lacceptance/RecipeFileObserver;->long_text(Ljava/lang/String;)V
    goto :pass
  :unknown
    const/16 v0, 84
    invoke-static {v0}, Ljava/lang/System;->exit(I)V
  :pass
    sget-object v0, Ljava/lang/System;->out:Ljava/io/PrintStream;
    const-string v1, "PASS recipe file phase "
    invoke-virtual {v1, v2}, Ljava/lang/String;->concat(Ljava/lang/String;)Ljava/lang/String;
    move-result-object v1
    invoke-virtual {v0, v1}, Ljava/io/PrintStream;->println(Ljava/lang/String;)V
    return-void
.end method

