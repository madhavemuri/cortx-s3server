/*
 * COPYRIGHT 2016 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author:  Arjun Hariharan <arjun.hariharan@seagate.com>
 * Original creation date: 11-Feb-2016
 */
package com.seagates3.jcloudclient;

import com.google.gson.Gson;
import com.google.gson.internal.LinkedTreeMap;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.UnsupportedEncodingException;
import java.lang.reflect.Constructor;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.HashMap;
import org.apache.commons.cli.BasicParser;
import org.apache.commons.cli.CommandLine;
import org.apache.commons.cli.HelpFormatter;
import org.apache.commons.cli.Options;
import org.apache.commons.cli.ParseException;

public class JCloudClient {

    private static final String CLASS_ACTION_FILE_NAME = "/class_action.json";
    private static final String PACKAGE_NAME = "com.seagates3.jcloudclient";
    private static Options s3Options;
    private static CommandLine cmd;

    public static void main(String[] args) throws FileNotFoundException,
            UnsupportedEncodingException {
        JCloudClient.run(args);
    }

    public static void run(String[] args) throws FileNotFoundException,
            UnsupportedEncodingException {
        init(args);
        if (cmd.hasOption("h")) {
            showUsage();
        }

        String[] commandArguments = cmd.getArgs();
        if (commandArguments.length == 0) {
            System.err.println("Incorrect command. Check usage");
            System.exit(1);
        }

        InputStream in = JCloudClient.class.
                getResourceAsStream(CLASS_ACTION_FILE_NAME);
        InputStreamReader reader = new InputStreamReader(in, "UTF-8");

        Gson gson = new Gson();
        HashMap<String, Object> gsonObj = gson.fromJson(reader, HashMap.class);

        LinkedTreeMap<String, String> classActionMapping
                = (LinkedTreeMap<String, String>) gsonObj.get(commandArguments[0]);

        if (classActionMapping == null) {
            System.err.println("Incorrect command");
            showUsage();
        }

        String classPath = getClassPath(classActionMapping.get("Class"));
        Class<?> clazz;
        Constructor<?> classConstructor;
        Method method;
        Object obj;

        try {

            clazz = Class.forName(classPath);
            classConstructor = clazz.getConstructor(CommandLine.class);
            obj = classConstructor.newInstance(cmd);

            method = clazz.getMethod(classActionMapping.get("Action"));
            method.invoke(obj);
        } catch (ClassNotFoundException | NoSuchMethodException | SecurityException ex) {
            System.out.println(ex);
        } catch (IllegalAccessException | IllegalArgumentException |
                InvocationTargetException | InstantiationException ex) {
            System.out.println(ex);
        }

    }

    private static void init(String[] args) {
        s3Options = constructOptions();
        try {
            cmd = new BasicParser().parse(s3Options, args);
        } catch (ParseException ex) {
            System.err.println("Incorrect command.\n");
            showUsage();
        }
    }

    /**
     * Construct the options required for s3 java cli.
     *
     * @return Options
     */
    private static Options constructOptions() {
        Options options = new Options();
        options.addOption("x", "access-key", true, "Access key id")
                .addOption("y", "secret-key", true, "Secret key")
                .addOption("t", "session-token", true, "Session token")
                .addOption("p", "path_style", false, "Use Path style APIs")
                .addOption("m", "multi-part-chunk-size", true,
                        "Size of chunk in MB.")
                .addOption("a", "aws", false, "Run operation on AWS S3.")
                .addOption("h", "help", false, "Show usage");

        return options;
    }

    private static void showUsage() {
        String usage = "Commands:\n"
                + "  Make bucket\n"
                + "      java -jar jcloudclient.jar mb s3://BUCKET\n"
                + "  Remove bucket\n"
                + "      java -jar jcloudclient.jar rb s3://BUCKET\n"
                + "  Remove bucket if empty\n"
                + "      java -jar jcloudclient.jar rbifempty s3://BUCKET\n"
                + "  Count all objects in the bucket, excluding directory markers\n"
                + "      java -jar jcloudclient.jar count s3://BUCKET/\n"
                + "  Count all objects in the bucket directory, excluding directory markers\n"
                + "      java -jar jcloudclient.jar countdir s3://BUCKET/ DIRECTORY\n"
                + "  List objects or buckets\n"
                + "      java -jar jcloudclient.jar ls [s3://BUCKET[/PREFIX]]\n"
                + "  Put object into bucket\n"
                + "      java -jar jcloudclient.jar put FILE s3://BUCKET[/PREFIX]\n"
                + "  Get object from bucket\n"
                + "      java -jar jcloudclient.jar get s3://BUCKET/OBJECT LOCAL_FILE\n"
                + "  Delete object from bucket\n"
                + "      java -jar jcloudclient.jar del s3://BUCKET/OBJECT\n"
                + "  Delete multiple objects from bucket\n"
                + "      java -jar jcloudclient.jar multidel s3://BUCKET OBJECT1 Object2... ObjectN\n"
                + "  Head Object\n"
                + "      java -jar jcloudclient.jar head s3://BUCKET/PREFIX/OBJECT\n"
                + "  Bucket or Object exists\n"
                + "      java -jar jcloudclient.jar exists s3://BUCKET[/PREFIX/OBJECT]\n"
                + "  Create Directory\n"
                + "      java -jar jcloudclient.jar dir s3://BUCKET/PREFIX\n"
                + "  Delete Directory\n"
                + "      java -jar jcloudclient.jar deldir s3://BUCKET/PREFIX\n"
                + "  Delete Objects inside a bucket without deleting the bucket\n"
                + "      java -jar jcloudclient.jar clrb s3://BUCKET/\n"
                + "  Delete Objects inside a bucket directory without deleting the bucket\n"
                + "      java -jar jcloudclient.jar clrbdir s3://BUCKET DIR/\n"
                + "  Multipart upload\n"
                + "      java -jar jcloudclient.jar put FILE s3://BUCKET[/PREFIX] "
                + "-m [size of each chunk in MB]\n";
        HelpFormatter s3HelpFormatter = new HelpFormatter();

        s3HelpFormatter.printHelp(usage, s3Options);
        System.exit(0);
    }

    private static String getClassPath(String className) {
        return PACKAGE_NAME + "." + className;
    }
}