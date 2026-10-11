#pragma once

// Collections built into the firmware so InkVerse works without a card. Both
// are public-domain texts from InkVerse's collections/built.
namespace inkverse {

constexpr char BUILT_IN_POETRY[] = R"VERSE(VERSE 1
M title Classic English poetry
M attribution Public domain
%%
S I Wandered Lonely as a Cloud
A William Wordsworth
T I wandered lonely as a cloud
T That floats on high o'er vales and hills,
T When all at once I saw a crowd,
T A host, of golden daffodils;
T Beside the lake, beneath the trees,
T Fluttering and dancing in the breeze.
T
T Continuous as the stars that shine
T And twinkle on the milky way,
T They stretched in never-ending line
T Along the margin of a bay:
T Ten thousand saw I at a glance,
T Tossing their heads in sprightly dance.
%%
S "Hope" is the thing with feathers
A Emily Dickinson
T "Hope" is the thing with feathers -
T That perches in the soul -
T And sings the tune without the words -
T And never stops - at all -
T
T And sweetest - in the Gale - is heard -
T And sore must be the storm -
T That could abash the little Bird
T That kept so many warm -
%%
S Sonnet 18
A William Shakespeare
T Shall I compare thee to a summer's day?
T Thou art more lovely and more temperate:
T Rough winds do shake the darling buds of May,
T And summer's lease hath all too short a date;
T Sometime too hot the eye of heaven shines,
T And often is his gold complexion dimm'd;
T And every fair from fair sometime declines,
T By chance or nature's changing course untrimm'd;
T But thy eternal summer shall not fade,
T Nor lose possession of that fair thou ow'st;
T Nor shall death brag thou wander'st in his shade,
T When in eternal lines to time thou grow'st:
T So long as men can breathe or eyes can see,
T So long lives this, and this gives life to thee.
%%
S Stopping by Woods on a Snowy Evening
A Robert Frost
T Whose woods these are I think I know.
T His house is in the village though;
T He will not see me stopping here
T To watch his woods fill up with snow.
T
T The woods are lovely, dark and deep,
T But I have promises to keep,
T And miles to go before I sleep,
T And miles to go before I sleep.
%%
S The Tyger
A William Blake
T Tyger Tyger, burning bright,
T In the forests of the night;
T What immortal hand or eye,
T Could frame thy fearful symmetry?
T
T In what distant deeps or skies,
T Burnt the fire of thine eyes?
T On what wings dare he aspire?
T What the hand, dare seize the fire?
)VERSE";

constexpr char BUILT_IN_STOIC[] = R"VERSE(VERSE 1
M title Stoic quotations
M attribution Public domain translations
%%
A Marcus Aurelius
B tr. George Long
T "The universe is change; our life is what our thoughts make
T it."
%%
A Marcus Aurelius
T "You have power over your mind - not outside events. Realise
T this and you will find strength."
%%
A Seneca
T "We suffer more often in imagination than in reality."
%%
A Seneca
B On the Shortness of Life
T "It is not that we have a short time to live but that we waste
T a lot of it."
%%
A Epictetus
T "First say to yourself what you would be; and then do what you
T have to do."
%%
A Epictetus
T "No man is free who is not master of himself."
%%
A Marcus Aurelius
T "Waste no more time arguing about what a good man should be.
T Be one."
%%
A Marcus Aurelius
B tr. George Long
T "The best revenge is to be unlike him who performed the
T injury."
%%
A Seneca
T "Luck is what happens when preparation meets opportunity."
%%
A Seneca
T "He who fears death will never do anything worthy of a man who
T is alive."
%%
A Epictetus
B Enchiridion
T "Do not seek for things to happen the way you want them to;
T rather wish that what happens happen the way it happens."
%%
A Marcus Aurelius
T "Very little is needed to make a happy life; it is all within
T yourself."
)VERSE";

}  // namespace inkverse
